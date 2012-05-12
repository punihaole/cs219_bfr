#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <math.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/ether.h>
#include <linux/if_packet.h>
#include <ifaddrs.h>

#include "cluster.h"
#include "bfr.h"
#include "bfrd.h"
#include "bfr_stats.h"

#include "grid.h"
#include "strategy.h"

#include "log.h"
#include "net_buffer.h"
#include "net_lib.h"
#include "linked_list.h"

#include "ts.h"
#include "thread_pool.h"

#include "bloom_filter.h"

#include "ccnu.h"

#define RAND_LEN 32

typedef enum {
    START,
    ELECTION,
    SEND_FILTER
} stategy_state_t;

struct bf_handler_arg
{
    struct bloom_msg bmsg;
    uint32_t nodeId;
};

struct strategy {
    unsigned long cluster_interval_ms;
    unsigned long bloom_interval_ms;
    unsigned long backoff_interval_ms;
    unsigned long cluster_freshness_interval_ms;
    unsigned long cluster_expiration_interval_ms;

    unsigned join_timeout;

    int max_join_attempts;

	int num_levels;
    struct timespec * ts_next_cluster; /* keep a timer for the election at each level */
    struct timespec ts_next_bloom;

    struct linked_list * route_msg_q;
    pthread_mutex_t lock; /* protects the route_msg_q when we want to atomically read the whole thing */
    pthread_cond_t cond;
    stategy_state_t state;

    int summary_size; /* num bits in our Bloom filters, calcualted by bfrd */

    thread_pool_t handler_pool;
    thread_pool_t nonce_pool;

    pthread_mutex_t nonce_lock;
    struct bitmap * seen_nonces;
};

/* the pending and matching lists keep track of join msgs we have seen.
 * upon receiving a cluster_response we check if there is a matching join.
 * if not, we send the response to strategy layer, if so we append it to
 * the response list.
 * This allows us to ignore joins that have already been responded to by
 * our neighbors.
 */
pthread_mutex_t pending_joins_lock;
struct linked_list * pending_joins;
pthread_mutex_t matching_responses_lock;
struct linked_list * matching_responses;

static struct ether_header eh[MAX_INTERFACES];

static struct strategy _strategy;

static int cluster(unsigned level);
static int bloom();

static long join_period(unsigned level, unsigned clusterId);
static long backoff_period();

static int create_bloom_msg(struct bloom_msg * msg,
                            unsigned origin_level, unsigned origin_clusterId,
                            unsigned dest_level, unsigned dest_clusterId,
                            double distance, struct bloom * bf);

static int broadcast_bloom_msg(struct bloom_msg * msg);
static int broadcast_join_msg(struct cluster_join_msg * msg);
static int broadcast_cluster_msg(struct cluster_msg * msg);
#ifdef BFR_USE_SLEEPING_PILLS
static int broadcast_pill_msg(struct sleeping_pill_msg * msg);
#endif

static int parse_as_bloom_msg(struct bfr_msg * msg, struct bloom_msg * bmsg);
static int parse_as_cluster_msg(struct bfr_msg * msg, struct cluster_msg * target);
static int parse_as_join_msg(struct bfr_msg * msg, struct cluster_join_msg * target);
#ifdef BFR_USE_SLEEPING_PILLS
static int parse_as_pill_msg(struct bfr_msg * msg, struct sleeping_pill_msg * target);
#endif

static void filter_msgs(struct linked_list * putIn, uint8_t type);

static void * handle_bloom_msg(void * arg);
static void * handle_cluster_join_msg(void * arg);

int strategy_init(unsigned num_levels, int bloom_interval_ms, int cluster_interval_ms)
{
	if (num_levels == 0)
		return -1;
    _strategy.cluster_interval_ms = cluster_interval_ms;
    _strategy.bloom_interval_ms = bloom_interval_ms;
    _strategy.backoff_interval_ms = DEFAULT_BACKOFF_INTERVAL_MS;
    _strategy.join_timeout = JOIN_TIMEOUT_MS;
    _strategy.max_join_attempts = JOIN_MAX_ATTEMPTS;

    _strategy.cluster_freshness_interval_ms = CLUSTER_HEAD_FRESHNESS_PERIOD_SEC * 1000;
    _strategy.cluster_expiration_interval_ms = CLUSTER_HEAD_EXPIRATION_PERIOD_SEC * 1000;

    _strategy.route_msg_q = linked_list_init(NULL);

    _strategy.num_levels = num_levels;
    _strategy.ts_next_cluster = malloc(sizeof(struct timespec) * num_levels);

    pthread_mutex_init(&_strategy.lock, NULL);
    pthread_cond_init(&_strategy.cond, NULL);

    pthread_mutex_init(&pending_joins_lock, NULL);
    pending_joins = linked_list_init(NULL);
    pthread_mutex_init(&matching_responses_lock, NULL);
    matching_responses = linked_list_init(NULL);

    struct node * my_node = &g_bfr.my_node;
    _strategy.summary_size = my_node->filter->vector->num_bits;
    log_print(g_log, "strategy_init: summary size = %d", _strategy.summary_size);

    if (tpool_create(&_strategy.handler_pool, DEFAULT_HANDLER_POOL) < 0) {
        return -1;
    }

    if (tpool_create(&_strategy.nonce_pool, DEFAULT_HANDLER_POOL) < 0) {
        return -1;
    }

    pthread_mutex_init(&_strategy.nonce_lock, NULL);
    _strategy.seen_nonces = bit_create(65535);

    g_faces = 0;
    struct ifaddrs * ifa, * p;

    if (getifaddrs(&ifa) != 0) {
        log_critical(g_log, "strategy_init: getifaddrs: %s", strerror(errno));
        return -1;
    }

    int family;
    for (p = ifa; p != NULL; p = p->ifa_next) {
        family = p->ifa_addr->sa_family;
        if ((p == NULL) || (p->ifa_addr == NULL)) continue;
        if (family != AF_PACKET) continue;
        if (strcmp(p->ifa_name, "lo") == 0) continue;

        struct ifreq if_mac;
        memset(&if_mac, 0, sizeof(struct ifreq));
        strncpy(if_mac.ifr_name, p->ifa_name, IFNAMSIZ-1);
        if (ioctl(g_sockfd, SIOCGIFHWADDR, &if_mac) < 0) {
            log_critical(g_log, "strategy_init: ioctl: %s", strerror(errno));
            return -1;
        }

        /* dest: broadcast MAC addr
         * src: my NIC
         */
        memset(&eh[g_faces], 0, sizeof(struct ether_header));
        eh[g_faces].ether_shost[0] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[0];
        eh[g_faces].ether_shost[1] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[1];
        eh[g_faces].ether_shost[2] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[2];
        eh[g_faces].ether_shost[3] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[3];
        eh[g_faces].ether_shost[4] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[4];
        eh[g_faces].ether_shost[5] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[5];
        eh[g_faces].ether_dhost[0] = 0xff;
        eh[g_faces].ether_dhost[1] = 0xff;
        eh[g_faces].ether_dhost[2] = 0xff;
        eh[g_faces].ether_dhost[3] = 0xff;
        eh[g_faces].ether_dhost[4] = 0xff;
        eh[g_faces].ether_dhost[5] = 0xff;
        eh[g_faces].ether_type = htons(BFR_ETHER_PROTO);

        memset(&g_eth_addr[g_faces], 0, sizeof(struct sockaddr_ll));
        g_eth_addr[g_faces].sll_ifindex = if_nametoindex(p->ifa_name);
        g_eth_addr[g_faces].sll_halen = ETH_ALEN;
        g_eth_addr[g_faces].sll_addr[0] = 0xff;
        g_eth_addr[g_faces].sll_addr[1] = 0xff;
        g_eth_addr[g_faces].sll_addr[2] = 0xff;
        g_eth_addr[g_faces].sll_addr[3] = 0xff;
        g_eth_addr[g_faces].sll_addr[4] = 0xff;
        g_eth_addr[g_faces].sll_addr[5] = 0xff;
        g_faces++;
    }

    return 0;
}

void strategy_close()
{
}

struct timespec * calc_next_event(struct linked_list * events)
{
	struct timespec * ts_earliest = (struct timespec *) linked_list_get(events, 0);
	struct timespec * ts_event;
	int i;
	for (i = 1; i < events->len; i++) {
		ts_event = (struct timespec *) linked_list_get(events, i);
		if (ts_compare(ts_event, ts_earliest) < 0) {
			ts_earliest = ts_event;
		}
	}

	return ts_earliest;
}

void * strategy_service(void * ignore)
{
    prctl(PR_SET_NAME, "strategy", 0, 0, 0);
    //log_print(g_log, "strategy_service: starting...");

    bloom();

    struct bfr_msg * msg = NULL;
    struct timespec * next_event = &_strategy.ts_next_bloom;
    struct linked_list * events = linked_list_init(NULL);
    linked_list_append(events, &_strategy.ts_next_bloom);
    int i;

    for (i = _strategy.num_levels; i > 0; i--) {
    	linked_list_append(events, &_strategy.ts_next_cluster[i-1]);
    	cluster(i);
    }

    next_event = calc_next_event(events);
    time_t rawtime;
	time(&rawtime);
	//unsigned long diff = next_event->tv_sec - rawtime;
	//log_print(g_log, "strategy_service: scheduled next event in %u seconds", diff);

    int rv;
    while (1) {
        pthread_mutex_lock(&_strategy.lock);
            while (!msg) {
                /* when a message is passed down from the upper layer they
                 * signal us.
                 */
                rv = pthread_cond_timedwait(&_strategy.cond, &_strategy.lock, next_event);
                if (rv == ETIMEDOUT) {
                    break;
                }
                msg = (struct bfr_msg * ) linked_list_remove(_strategy.route_msg_q, 0);
            }
        pthread_mutex_unlock(&_strategy.lock);

        /* we retrieved  a message or timedout*/
        if (rv == ETIMEDOUT) {
            struct timespec now;
            ts_fromnow(&now);

            for (i = 1; i < events->len; i++) {
	            if (ts_compare(&now, &_strategy.ts_next_cluster[i-1]) > 0) {
    	            cluster(i);
    	        }
    	    }

            if (ts_compare(&now, &_strategy.ts_next_bloom) > 0) {
                bloom();
            }

            next_event = calc_next_event(events);
			time_t rawtime;
			time(&rawtime);
			//unsigned long diff = next_event->tv_sec - rawtime;
			//log_print(g_log, "strategy_service: scheduled next event in %u seconds", diff);

            /* check if we got any messages as well */
            pthread_mutex_lock(&_strategy.lock);
            msg = (struct bfr_msg * ) linked_list_remove(_strategy.route_msg_q, 0);
            pthread_mutex_unlock(&_strategy.lock);
        }

        while (msg) {
            if (msg->hdr.type == MSG_NET_BLOOMFILTER_UPDATE) {
                struct bf_handler_arg * barg = malloc(sizeof(struct bf_handler_arg));
                barg->nodeId = msg->hdr.nodeId;
                barg->bmsg.vector = NULL;

                if (parse_as_bloom_msg(msg, &barg->bmsg) < 0) {
                    //log_print(g_log, "strategy_service: parse_as_bloom_msg failed.");
                } else {
                    pthread_mutex_lock(&_strategy.nonce_lock);
                    int seen = bit_test(_strategy.seen_nonces, barg->bmsg.nonce);
                    if (!seen) {
                        bit_set(_strategy.seen_nonces, barg->bmsg.nonce);
                    }
                    pthread_mutex_unlock(&_strategy.nonce_lock);
                    if (!seen) {
                        tpool_add_job(&_strategy.handler_pool, handle_bloom_msg, barg, TPOOL_NO_RV, NULL, NULL);
                    } else {
                        log_debug(g_log, "strategy_service: dropping bloom with duplicate nonce (%u)", barg->bmsg.nonce);
                    }
                }
            } else if (msg->hdr.type == MSG_NET_CLUSTER_JOIN) {
                struct cluster_join_msg * jmsg = malloc(sizeof(struct cluster_join_msg));
                if (parse_as_join_msg(msg, jmsg) < 0) {
                    //log_print(g_log, "strategy_service: parse_as_join_msg failed.");
                } else {
                    tpool_add_job(&_strategy.handler_pool, handle_cluster_join_msg, jmsg, TPOOL_NO_RV, NULL, NULL);
                }
            } else if (msg->hdr.type == MSG_NET_CLUSTER_RESPONSE) {
                /* we have to service CLUSTER RESPONSES here (not in a separate
                 * thread so we don't consume a response the cluster() routine
                 * should consume.
                 * We check if one of these responses matches a pending join that
                 * we are planning on answering. This is to prevent a bunch of nodes
                 * from transmitting a response as soon as they hear a join.
                 */
                struct cluster_msg * response;
                response = (struct cluster_msg * ) malloc(sizeof(struct cluster_msg));
                if (parse_as_cluster_msg(msg, response) < 0) {
                    //log_print(g_log, "strategy_service: failed to parse MSG_NET_CLUSTER_RESPONSE -- IGNORING");
                } else {
                    bfrstat_rcvd_cluster(response);

                    /* see if the response matches any of the joins that are pending */
                    int found = 0;
                    int i;
                    pthread_mutex_lock(&pending_joins_lock);
                        for (i = 0; i < pending_joins->len; i++) {
                            struct cluster_join_msg * join = linked_list_get(pending_joins, i);
                            if (join->cluster_id == response->cluster_id &&
                                join->cluster_level == response->cluster_level) {
                                found = 1;
                                break;
                            }
                        }
                    pthread_mutex_unlock(&pending_joins_lock);

                    if (found) {
                        pthread_mutex_lock(&matching_responses_lock);
                            linked_list_append(matching_responses, response);
                        pthread_mutex_unlock(&matching_responses_lock);
                    } else {
                        free(response);
                    }
                }

            } else if (msg->hdr.type == MSG_NET_SLEEPING_PILL) {
            	/* TODO */
            } else {
                log_print(g_log, "strategy_service: rcvd msg of type %d. Not our responsiblity!",
                          msg->hdr.type);
            }

            free(msg->payload.data);
            free(msg);
            pthread_mutex_lock(&_strategy.lock);
                msg = linked_list_remove(_strategy.route_msg_q, 0);
            pthread_mutex_unlock(&_strategy.lock);
        }
    }

    pthread_exit(NULL);
}

int strategy_passMsg(struct bfr_msg * msg)
{
    /* we don't want the daemon to interfere with us if we are atomically
     * reading the entire queue
     */
    pthread_mutex_lock(&_strategy.lock);
        linked_list_append(_strategy.route_msg_q, (void * ) msg);
        pthread_cond_signal(&_strategy.cond);
    pthread_mutex_unlock(&_strategy.lock);

    return 0;
}

/* figures out who our cluster head is */
static int cluster(unsigned level)
{
    long wait;
    struct cluster_join_msg join;
    join.cluster_level = level;
    join.cluster_id = clus_get_clusterId(join.cluster_level);
    uint32_t cluster_head = g_bfr.nodeId; /* we declare ourself a cluster head if no one responds */
    struct linked_list * cluster_responses = linked_list_init((delete_t)bfr_msg_delete);
    struct bfr_msg * msg;
    int distance = 0;

    //log_print(g_log, "cluster(%d): beginning procedure for %u:%u...", level, join.cluster_level, join.cluster_id);

    wait = backoff_period();
    //log_print(g_log, "cluster(%d): waiting for %ld ms.", level, wait);
    msleep(wait);

    pthread_mutex_lock(&g_bfr.bfr_lock);

    int attempt;
    for (attempt = 0; attempt < _strategy.max_join_attempts; attempt++) {
        if (attempt > 1) {
            /* backoff incase join failed from congestion */
            wait = backoff_period();
            msleep(wait);
            //log_print(g_log, "cluster(%d): retrying join attempt.", level);
        }

        broadcast_join_msg(&join);
        wait = join_period(join.cluster_level, join.cluster_id);
        //log_print(g_log, "cluster(%d): waiting for response for %ld ms.", level, wait);
        msleep(wait);
        filter_msgs(cluster_responses, MSG_NET_CLUSTER_RESPONSE);

        int i;
        if (cluster_responses->len > 0) {
            bool_t found = FALSE;
            pthread_mutex_lock(&_strategy.lock);
            uint32_t candidate_id = 0xffffffff;
            for (i = 0; i < cluster_responses->len; i++) {
                msg = (struct bfr_msg * )linked_list_remove(cluster_responses, 0);
                struct cluster_msg response;
                memset(&response, 0, sizeof(struct cluster_msg));
                parse_as_cluster_msg(msg, &response);

                if ((response.cluster_level == join.cluster_level) &&
                    (response.cluster_id == join.cluster_id) &&
                    (response.cluster_id < candidate_id)) {
                    //log_print(g_log, "cluster(%d): got response, cluster head = %u.", level, response.cluster_head);
                    cluster_head = response.cluster_head;
                    candidate_id = cluster_head;
                    distance = response.hops;
                    free(msg->payload.data);
                    free(msg);
                    msg = NULL;
                    found = TRUE;
                } else {
                    /* doesn't pertain to us, put it back in the queue */

                    linked_list_append(_strategy.route_msg_q, (void * ) msg);
                }
            }
            pthread_mutex_unlock(&_strategy.lock);
            if (found)
                goto FOUND_CLUSTER_HEAD;
        }
        /* no response received */
    }

    FOUND_CLUSTER_HEAD:
    linked_list_delete(cluster_responses);

    if (level == _strategy.num_levels) {
		g_bfr.leaf_head.nodeId = cluster_head;
		ts_fromnow(&g_bfr.leaf_head.expiration);
		ts_fromnow(&g_bfr.leaf_head.stale);
		ts_addms(&g_bfr.leaf_head.expiration,
		         _strategy.cluster_freshness_interval_ms);
		ts_addms(&g_bfr.leaf_head.stale,
		         _strategy.cluster_freshness_interval_ms);

		//log_print(g_log, "cluster(%d): set leaf cluster head to %u.", level, cluster_head);
		//log_print(g_log, "cluster(%d): distance from head = %u.", level, distance);

		g_bfr.leaf_cluster.id = join.cluster_id;
		g_bfr.leaf_head.nodeId = cluster_head;
		g_bfr.leaf_head.clusterId = join.cluster_id;
		g_bfr.leaf_head.distance = distance;
	} else {
		struct cluster * _cluster = NULL;

		if (clus_get_cluster(level, clus_get_clusterId(level), &_cluster) < 0) {
			_cluster = malloc(sizeof(struct cluster));
			_cluster->nodes = NULL;
			_cluster->agg_filter = bloom_create(_strategy.summary_size, BLOOM_ARGS);
			_cluster->id = clus_get_clusterId(level);
			_cluster->level = level;
			//log_print(g_log, "cluster: adding my cluster (%u:%u)", _cluster->level, _cluster->id);
			clus_add_cluster(_cluster);
		}
	}

    if (cluster_head == g_bfr.nodeId) {
    	if (level == _strategy.num_levels ) {
			/* we are the leaf cluster head */
		    g_bfr.leaf_cluster.agg_filter = bloom_create(_strategy.summary_size, BLOOM_ARGS);
		    g_bfr.leaf_head.distance = 0;
    	} else {
			/* we are the cluster head for this level */
			struct cluster * _cluster = NULL;
			clus_get_cluster(level, clus_get_clusterId(level), &_cluster);
    	}
    	bit_set(g_bfr.is_cluster_head, level-1);
    	//log_print(g_log, "cluster: setting is_cluster_head[%d]", level-1);
    } else {
        bit_clear(g_bfr.is_cluster_head, level-1);
    }

    pthread_mutex_unlock(&g_bfr.bfr_lock);

    ts_fromnow(&_strategy.ts_next_cluster[level - 1]);
    ts_addms(&_strategy.ts_next_cluster[level - 1], _strategy.cluster_interval_ms);
    //log_print(g_log, "cluster(%d): done.", level);

    return 0;
}

static double frac_change(struct bloom * a, struct bloom * b)
{
    int diff = bit_diff(a->vector, b->vector);
    double frac = ((double) diff)  / ((double) a->size);

    return frac;
}

static int bloom()
{
    //log_print(g_log, "bloom: beginning procedure...");

    struct bloom * filter = NULL;
    /* update our bloom filter */
    if (ccnu_cs_summary(&filter) != 0) {
        //log_print(g_log, "bloom: failed to retrieve Bloom filter from CS daemon!");
        return -1;
    }

    long wait = backoff_period();
    //log_print(g_log, "bloom: waiting for %lu ms.", wait);
    msleep(wait);

    pthread_mutex_lock(&g_bfr.bfr_lock);
    struct bloom * old_filter = g_bfr.my_node.filter;
    double frac = frac_change(old_filter, filter);
    bloom_destroy(old_filter);
    g_bfr.my_node.filter = filter;

    int i;
    for (i = g_bfr.num_levels; i > 0; i--) {
        unsigned level = i;
        unsigned clusterId = clus_get_clusterId(level);

        if (bit_test(g_bfr.is_cluster_head, level - 1)) {
            //log_print(g_log, "bloom: I am the cluster head for %u:%u", level, clusterId);
            /* I am the cluster head */
            /* create an aggregate filter */
            struct cluster * _cluster = NULL;

            if (clus_get_cluster(level, clusterId, &_cluster) < 0) {
                log_print(g_log, "bloom: I am the cluster head for %u:%u and could not compute aggregate filter!",
                       level, clusterId);
                continue;
            }
            if (clus_compute_aggregate(_cluster) != 0) {
                log_print(g_log, "bloom: I am the cluster head for %u:%u and could not compute aggregate filter!",
                       level, clusterId);
                continue;
            };

            bloom_or(_cluster->agg_filter, filter, _cluster->agg_filter);

            if (bit_allClear(_cluster->agg_filter->vector)) {
                // the filter is empty, no need to transmit
                continue;
            }

            struct bloom_msg msg;
            msg.vector = NULL;
            double distance;
            if (level > 1) {
                unsigned parent_clusterId = clus_get_clusterId(level - 1);
                if (grid_distance(level-1, parent_clusterId, g_bfr.x, g_bfr.y, &distance) != 0) {
                    log_print(g_log, "bloom: failed to calculate distance to %u:%u",
                              level, parent_clusterId);
                    continue;
                }
                //log_print(g_log, "bloom: calculated distance to %u:%u = %5.5f", level, parent_clusterId, distance);

                /* we're sending a bloom msg to our parent cluster head */
                if (!bit_test(g_bfr.is_cluster_head, i - 1)) {
                    /* send our aggegated filter up the chain if we are not the next head */
                    //log_print(g_log, "bloom: forwarding aggregate filter to level %u cluster head", i-1);
                    create_bloom_msg(&msg, level, clusterId, level-1, parent_clusterId, distance, _cluster->agg_filter);
                    broadcast_bloom_msg(&msg);
                    free(msg.vector);
                    msg.vector = NULL;
                }
            }

            #ifndef BFR_FLOOD_STATE
            //Send to neighboring cluster Ids
            unsigned neighbors[3];
            grid_3neighbors(level, clusterId, neighbors);
            int k;
            for (k = 0; k < 3; k++) {
                if (grid_distance(level, neighbors[k], g_bfr.x, g_bfr.y, &distance) != 0) {
                    log_print(g_log, "bloom: failed to calculate distance to %u:%u",
                              level, neighbors[k]);
                    continue;
                }
                log_print(g_log, "bloom: forwarding aggregate filter to neighbor %u:%u", level, neighbors[k]);
                create_bloom_msg(&msg, level, clusterId, level, neighbors[k], distance, _cluster->agg_filter);
                broadcast_bloom_msg(&msg);
                free(msg.vector);
                msg.vector = NULL;
            }
            #else
            //destination is treated as TTL
            create_bloom_msg(&msg, level, clusterId, 5, 0, distance, _cluster->agg_filter);
            broadcast_bloom_msg(&msg);
            free(msg.vector);
            #endif
        } else {
            /* I am not the cluster head */
            if (bit_allClear(filter->vector)) {
                //my filter is empty, no need to tx
                continue;
            }
            struct bloom_msg msg;
            msg.vector = NULL;
            double distance = -1 * g_bfr.leaf_head.distance - 1;
            /* we're sending a bloom msg to our own cluster head */
            //log_print(g_log, "bloom: forwarding aggregate filter to cluster head");
            create_bloom_msg(&msg, level, clusterId, level, clusterId, distance, filter);
            broadcast_bloom_msg(&msg);
            free(msg.vector);
        }
    }
    pthread_mutex_unlock(&g_bfr.bfr_lock);

    ts_fromnow(&_strategy.ts_next_bloom);
    /* we tweak the rate we share the next bloom filter based on how much it changed */
    int interval = _strategy.bloom_interval_ms -
                   (int) (((double)_strategy.bloom_interval_ms / 2.0) * frac);
    ts_addms(&_strategy.ts_next_bloom, interval);

    //log_print(g_log, "bloom: done.");

    return 0;
}

static uint16_t gen_nonce()
{
    char * dev = "/dev/urandom";
    int fp = open(dev, O_RDONLY);
    uint16_t rand;

    if (!fp) {
        //log_print(g_log, "read_rand: could not open %s.", dev);
        return -1;
    }

    if (read(fp, &rand, sizeof(uint16_t)) != sizeof(uint16_t)) {
        //log_print(g_log, "read_rand: read failed - %s", strerror(errno));
        close(fp);
        return -1;
    }

    close(fp);
    return rand;
}

static long join_period(unsigned level, unsigned clusterId)
{
    long rand = 0;

    if (read_rand("/dev/urandom", &rand) != 0) {
        //log_print(g_log, "cluster: failed to generate a random!");
    }

    rand = abs(rand % _strategy.join_timeout);
    double dist_from_center = 0;
    if (grid_distance(level, clusterId, g_bfr.x, g_bfr.y, &dist_from_center) < 0)
        return _strategy.join_timeout;

    double grid_x = 1.0, grid_y = 1.0;
    if (grid_dimensions(level, &grid_x, &grid_y) < 0)
        return _strategy.join_timeout;

    double scale_fac = dist_from_center / (sqrt(pow(grid_x, 2.0) + pow(grid_y, 2.0)) / 2.0);
    double wait = (double)rand * scale_fac;

    return (long) wait;
}

static long backoff_period()
{
    long wait;

    if (read_rand("/dev/urandom", &wait) != 0) {
        return _strategy.backoff_interval_ms;
    }

    wait = abs(wait % _strategy.backoff_interval_ms);

    return wait;
}

static int parse_as_bloom_msg(struct bfr_msg * msg, struct bloom_msg * bmsg)
{
    if (!msg || !bmsg)
        return -1;

    if (msg->hdr.type != MSG_NET_BLOOMFILTER_UPDATE) return -1;

    if (msg->hdr.payload_size < BLOOM_MSG_MIN_SIZE) {
        log_print(g_log, "parse_as_bloom_msg: hdr payload size smaller than expected (got %u, expecting >= %u) -- IGNORING!",
               msg->hdr.payload_size, (unsigned) BLOOM_MSG_MIN_SIZE);
        return -1;
    }

    struct net_buffer * buffer;
    buffer = net_buffer_createFrom(msg->payload.data,
                                   msg->hdr.payload_size);
    bmsg->nonce = net_buffer_getShort(buffer);
    bmsg->origin_level = net_buffer_getByte(buffer);
    bmsg->origin_clusterId = net_buffer_getShort(buffer);
    bmsg->dest_level = net_buffer_getByte(buffer);
    bmsg->dest_clusterId = net_buffer_getShort(buffer);
    bmsg->lastHopDistance = net_buffer_getLong(buffer);
    bmsg->vector_bits = net_buffer_getShort(buffer);

    int vec_size = (int)ceil((double)(bmsg->vector_bits) / BITS_PER_WORD);
    bmsg->vector = (uint32_t * ) malloc(vec_size * sizeof(uint32_t));
    net_buffer_copyFrom(buffer, bmsg->vector, vec_size * sizeof(uint32_t));

    net_buffer_delete(buffer);

    return 0;
}

static int parse_as_cluster_msg(struct bfr_msg * msg,
                                struct cluster_msg * target)
{
    if (!msg || !target) return -1;

    if (msg->hdr.type != MSG_NET_CLUSTER_RESPONSE) return -1;

    struct net_buffer * buffer = net_buffer_createFrom(msg->payload.data,
                                                       msg->hdr.payload_size);

    target->cluster_level = net_buffer_getByte(buffer);
    target->cluster_id = net_buffer_getShort(buffer);
    target->cluster_head = net_buffer_getInt(buffer);
    target->hops = net_buffer_getByte(buffer);

    net_buffer_delete(buffer);

    return 0;
}

#ifdef BFR_USE_SLEEPING_PILLS
static int parse_as_pill_msg(struct bfr_msg * msg, struct sleeping_pill_msg * target)
{
	if (!msg || !target) return -1;

	if (msg->hdr.type != MSG_NET_SLEEPING_PILL) return -1;

	struct net_buffer * buffer = net_buffer_createFrom(msg->payload.data,
	                                                   msg->hdr.payload_size);
	target->level = net_buffer_getByte(buffer);
	target->clusterId = net_buffer_getShort(buffer);
	target->clusterHead = net_buffer_getInt(buffer);
	target->hopCount = net_buffer_getByte(buffer);

	net_buffer_delete(buffer);

	return 0;
}
#endif

static int create_bloom_msg(struct bloom_msg * msg,
                            unsigned origin_level, unsigned origin_clusterId,
                            unsigned dest_level, unsigned dest_clusterId,
                            double distance, struct bloom * bf)
{
    if (!msg) {
        //log_print(g_log, "create_bloom_msg: msg not allocated -- IGNORING!");
        return -1;
    }

    if (msg->vector) {
        //log_print(g_log, "create_bloom_msg: vector already allocated? Do not do this...continuing with caution.");
    }

    if (!bf) {
        //log_print(g_log, "create_bloom_msg: Bloom filter not allocated -- IGNORING!");
        return -1;
    }

    if ((origin_level > 0xff) || (origin_clusterId > 0xffff) ||
        (dest_level > 0xff) || (dest_clusterId > 0xffff)) {
        //log_print(g_log, "create_bloom_msg: invalid parameter value(s) -- IGNORING");
        return -1;
    }

    msg->nonce = gen_nonce();
    msg->origin_level = (uint8_t) origin_level;
    msg->origin_clusterId = (uint16_t) origin_clusterId;
    msg->dest_level = (uint8_t) dest_level;
    msg->dest_clusterId = (uint16_t) dest_clusterId;
    msg->lastHopDistance = (uint64_t) pack_ieee754_64(distance);
    msg->vector_bits = (uint16_t) bf->vector->num_bits;
    int vec_len = (int) ceil((double)(msg->vector_bits) / BITS_PER_WORD);
    msg->vector = (uint32_t * ) malloc(vec_len * sizeof(uint32_t));

    if (!msg->vector) {
        //log_print(g_log, "create_bloom_msg: couldn't allocate vector %s.", strerror(errno));
        return -1;
    }

    memcpy(msg->vector, bf->vector->map, vec_len * sizeof(uint32_t));
    return 0;
}

static int broadcast_bloom_msg(struct bloom_msg * msg)
{
    if (!msg) {
        //log_print(g_log, "send_bloom: tried to send NULL packet -- IGNORING!");
        return -1;
    }

    if (!msg->vector) {
        //log_print(g_log, "send_bloom: tried to send Bloom filter with no vector -- IGNORING!");
        return -1;
    }

    bfrstat_sent_bloom(msg);

    int size = BLOOM_MSG_MIN_SIZE;
    int vec_size = (int)ceil((double)msg->vector_bits / 8.0);
    size += vec_size;

    struct net_buffer buf;
    net_buffer_init(BFR_MAX_PACKET_SIZE, &buf);

    int i;
    int rv = 0;
    for (i = 0; i < g_faces; i++) {
        net_buffer_reset(&buf);
        /* ether header */
        net_buffer_copyTo(&buf, &eh[i], sizeof(struct ether_header));

        /* pack the header */
        net_buffer_putByte(&buf, MSG_NET_BLOOMFILTER_UPDATE);
        net_buffer_putInt(&buf, g_bfr.nodeId);
        net_buffer_putInt(&buf, size);
        /*pack the payload (bloom filter msg) */
        net_buffer_putShort(&buf, msg->nonce);
        net_buffer_putByte(&buf, msg->origin_level);
        net_buffer_putShort(&buf, msg->origin_clusterId);
        net_buffer_putByte(&buf, msg->dest_level);
        net_buffer_putShort(&buf, msg->dest_clusterId);
        net_buffer_putLong(&buf, msg->lastHopDistance);
        net_buffer_putShort(&buf, msg->vector_bits);
        net_buffer_copyTo(&buf, msg->vector, vec_size);

        int n = buf.buf_ptr - buf.buf;
        int sent = sendto(g_sockfd, buf.buf, n, 0, (struct sockaddr *)&g_eth_addr[i], sizeof(g_eth_addr[i]));
        if (sent == -1) {
            log_error(g_log, "broadcast_bloom_msg: sendto(%d): %s", i, strerror(errno));
            rv = -1;
        } else if (sent != n) {
            log_warn(g_log, "broadcast_bloom_msg: warning sent %d bytes, expected %d!", sent, n);
        }
    }

    free(buf.buf);

    return rv;
}

static int broadcast_join_msg(struct cluster_join_msg * msg)
{
    if (!msg) {
        //log_print(g_log, "broadcast_join_msg: tried to send NULL packet -- IGNORING!");
        return -1;
    }

    bfrstat_sent_join(msg);

    int size = CLUSTER_JOIN_MSG_SIZE;

    struct net_buffer buf;
    net_buffer_init(size + HDR_SIZE, &buf);

    int i;
    int rv = 0;
    for (i = 0; i < g_faces; i++) {
        net_buffer_reset(&buf);
        /* ether header */
        net_buffer_copyTo(&buf, &eh[i], sizeof(struct ether_header));

        /* header */
        net_buffer_putByte(&buf, MSG_NET_CLUSTER_JOIN);
        net_buffer_putInt(&buf, g_bfr.nodeId);
        net_buffer_putInt(&buf, size);

        /* payload */
        net_buffer_putByte(&buf, msg->cluster_level);
        net_buffer_putShort(&buf, msg->cluster_id);

        int n = buf.buf_ptr - buf.buf;
        int sent = sendto(g_sockfd, buf.buf, n, 0, (struct sockaddr *)&g_eth_addr[i], sizeof(g_eth_addr[i]));
        if (sent == -1) {
            log_error(g_log, "ccnudnb_fwd_data: sendto(%d): %s", i, strerror(errno));
            rv = -1;
        } else if (sent != n) {
            log_warn(g_log, "ccnudnb_fwd_data: warning sent %d bytes, expected %d!", sent, n);
        }
    }

    free(buf.buf);

    return rv;
}

static int broadcast_cluster_msg(struct cluster_msg * msg)
{
    if (!msg) {
        //log_print(g_log, "broadcast_cluster_msg: tried to send NULL packet -- IGNORING!");
        return -1;
    }

    bfrstat_sent_cluster(msg);

    int size = CLUSTER_RESPONSE_MSG_SIZE;
    struct net_buffer buf;
    net_buffer_init(size + HDR_SIZE, &buf);

    int i;
    int rv = 0;
    for (i = 0; i < g_faces; i++) {
        net_buffer_reset(&buf);
        /* ether header */
        net_buffer_copyTo(&buf, &eh[i], sizeof(struct ether_header));

        /* header */
        net_buffer_putByte(&buf, MSG_NET_CLUSTER_RESPONSE);
        net_buffer_putInt(&buf, g_bfr.nodeId);
        net_buffer_putInt(&buf, size);
        /* payload */
        net_buffer_putByte(&buf, msg->cluster_level);
        net_buffer_putShort(&buf, msg->cluster_id);
        net_buffer_putInt(&buf, msg->cluster_head);
        net_buffer_putByte(&buf, msg->hops);

        int n = buf.buf_ptr - buf.buf;
        int sent = sendto(g_sockfd, buf.buf, n, 0, (struct sockaddr *)&g_eth_addr[i], sizeof(g_eth_addr[i]));
        if (sent == -1) {
            log_error(g_log, "ccnudnb_fwd_data: sendto(%d): %s", i, strerror(errno));
            rv = -1;
        } else if (sent != n) {
            log_warn(g_log, "ccnudnb_fwd_data: warning sent %d bytes, expected %d!", sent, n);
        }
    }

    free(buf.buf);

    return rv;
}

#ifdef BFR_USE_SLEEPING_PILLS
static int broadcast_pill_msg(struct sleeping_pill_msg * msg)
{
	if (!msg) {
        //log_print(g_log, "broadcast_pill_msg: tried to send NULL packet -- IGNORING!");
        return -1;
    }

    int size = SLEEPING_PILL_MSG_SIZE;
    struct net_buffer buf;
    net_buffer_init(size + HDR_SIZE, &buf);

    int i;
    int rv = 0;
    for (i = 0; i < g_faces; i++) {
        net_buffer_reset(&buf);
        /* ether header */
        net_buffer_copyTo(&buf, &eh[i], sizeof(struct ether_header));

        /* header */
        net_buffer_putByte(&buf, SLEEPING_PILL_MSG_SIZE);
        net_buffer_putInt(&buf, g_bfr.nodeId);
        net_buffer_putInt(&buf, size);

        /* payload */
        net_buffer_putByte(&buf, msg->level);
        net_buffer_putShort(&buf, msg->clusterId);
        net_buffer_putInt(&buf, msg->clusterHead);
        net_buffer_putByte(&buf, msg->hopCount);

        int n = buf.buf_ptr - buf.buf;
        int sent = sendto(g_sockfd, buf.buf, n, 0, (struct sockaddr *)&g_eth_addr[i], sizeof(g_eth_addr[i]));
        if (sent == -1) {
            log_error(g_log, "ccnudnb_fwd_data: sendto(%d): %s", i, strerror(errno));
            rv = 0;
        } else if (sent != n) {
            log_warn(g_log, "ccnudnb_fwd_data: warning sent %d bytes, expected %d!", sent, n);
        }
    }

    free(buf.buf);

    return rv;
}
#endif

static void filter_msgs(struct linked_list * putIn, uint8_t type)
{
    struct linked_list * save = linked_list_init(NULL);
    struct bfr_msg * msg = NULL;

    pthread_mutex_lock(&_strategy.lock);
    while(_strategy.route_msg_q->len > 0) {
        msg = (struct bfr_msg *) linked_list_remove(_strategy.route_msg_q, 0);

        if (msg->hdr.type != type) {
            linked_list_append(save, msg);
        } else {
            linked_list_append(putIn, msg);
        }
    }

    /* add the saved msgs back to the route_msg_q */
    while (save->len > 0) {
        msg = (struct bfr_msg *) linked_list_remove(save, 0);
        linked_list_append(_strategy.route_msg_q, msg);
    }

    pthread_mutex_unlock(&_strategy.lock);

    linked_list_delete(save);
}

static void * unregister_nonce(void * arg)
{
    uint16_t nonce;
    memcpy(&nonce, arg, sizeof(uint16_t));
    free(arg);

    msleep(100);
    pthread_mutex_lock(&_strategy.nonce_lock);
        bit_clear(_strategy.seen_nonces, nonce);
    pthread_mutex_unlock(&_strategy.nonce_lock);

    return NULL;
}

static void * handle_bloom_msg(void * arg)
{
    struct bf_handler_arg * barg = (struct bf_handler_arg * ) arg;
    struct bloom_msg * msg = &barg->bmsg;
    uint32_t origin_nodeId = barg->nodeId;
    uint16_t * nonce = malloc(sizeof(uint16_t));
    *nonce = msg->nonce;

    bfrstat_rcvd_bloom(msg);
    log_print(g_log, "handle_bloom_msg: from %u@%u:%u for (%u:%u), lhd = %5.2f.",
              origin_nodeId, msg->origin_level, msg->origin_clusterId,
              msg->dest_level, msg->dest_clusterId,
              unpack_ieee754_64(msg->lastHopDistance));
    struct bloom * filter = bloom_createFromVector(msg->vector_bits, msg->vector, BLOOM_ARGS);

    log_print(g_log, "filter->size = %d", filter->size);
    log_print(g_log, "filter->vector->num_words = %d", filter->vector->num_words);
    log_print(g_log, "filter->vector->num_bits = %d", filter->vector->num_bits);
    log_print(g_log, "\torigin_level=%d", msg->origin_level);
    log_print(g_log, "\torigin_clusterId=%d", msg->origin_clusterId);
    log_print(g_log, "\tdest_level=%d", msg->dest_level);
    log_print(g_log, "\tdest_clusterId=%d", msg->dest_clusterId);
    log_print(g_log, "\tlastHopDistance=%6.5f", msg->vector_bits);
    int words = filter->vector->num_words;
    char str[256];
    str[0] = 0;
    int i;
    for (i = 0; i < words; i++) {
        sprintf(str, "%s %04X", str, msg->vector[i]);
    }
    log_print(g_log, "\tvector = %s", str);

    /* we use overhead bloom filters */
    struct cluster * clus = NULL;
    bool_t fwd = FALSE;

    pthread_mutex_lock(&g_bfr.bfr_lock);

    bool_t intra = (msg->origin_level == msg->dest_level) &&
                       (msg->origin_clusterId == msg->dest_clusterId);
    bool_t myLeaf = (msg->dest_level == g_bfr.num_levels) &&
                    (msg->dest_clusterId == clus_leaf_clusterId());

    if (intra && !myLeaf) {
        //ignore it
        pthread_mutex_unlock(&g_bfr.bfr_lock);
        bloom_destroy(filter);
        goto CLEANUP;
    } else if (clus_get_cluster(msg->origin_level, msg->origin_clusterId, &clus) != 0) {
        /* we've never seen this cluster before, insert it in the tree */
        log_print(g_log, "handle_bloom_msg: discovered a new cluster (%u:%u).",
                  msg->origin_level, msg->origin_clusterId);
        clus = (struct cluster *) malloc(sizeof(struct cluster));
        clus->nodes = NULL;
        clus->agg_filter = filter;
        clus->id = msg->origin_clusterId;
        clus->level = msg->origin_level;
        if (clus_add_cluster(clus) < 0) {
            log_print(g_log, "handle_bloom_msg: failed to add new cluster? (%u:%u)",
                      clus->level, clus->id);
        }
    } else {
        if (intra && myLeaf) {
            //intra in my cluster
            //record that node
            struct node * _node = NULL;
            if (clus_get_node(msg->origin_clusterId, origin_nodeId, &_node) < 0) {
                _node = malloc(sizeof(struct node));
                _node->nodeId = origin_nodeId;
                _node->filter = filter;
                if (clus_add_node(msg->origin_clusterId, _node) < 0) {
                    log_print(g_log, "handle_bloom_msg: failed to add node (%u@%u)", origin_nodeId, msg->origin_clusterId);
                }
            } else if (_node) {
                bloom_destroy(_node->filter);
                _node->filter = filter;
            }
            log_print(g_log, "recording node filter (%u@%u:%u)", origin_nodeId, msg->origin_level, msg->origin_clusterId);
        } else {
            //inter, update that aggregate
            log_print(g_log, "updating aggregate filter (%u:%u)", clus->level, clus->id);
            bloom_destroy(clus->agg_filter);
            clus->agg_filter = filter;
        }
        //log_print(g_log, "handle_bloom_msg: previously seen cluster.");
    }

    #ifndef BFR_FLOOD_STATE
    /* figure out if we need to forward */
    if (msg->dest_clusterId == clus_get_clusterId(msg->dest_level)) {
        /* we are in the local cluster */
        if (bit_test(g_bfr.is_cluster_head, msg->dest_level - 1)) {
            /* we are the cluster head, msg destined to us */
            /*log_print(g_log, "handle_bloom_msg: rcvd aggregate filter for %u:%u.",
                      msg->origin_level, msg->origin_clusterId);*/
        } else {
            /* we need to forward to our cluster head */
            double lastHop = unpack_ieee754_64(msg->lastHopDistance);
            if (lastHop > 0) {
                /* we received it from another cluster. We make the num hops
                 * taken negative so we can differentiate */
                msg->lastHopDistance = pack_ieee754_64(-1 * g_bfr.leaf_head.distance);
                fwd = TRUE;
                //log_print(g_log, "handle_bloom: fwding bloom msg to cluster head, distance = %5.5", msg->lastHopDistance);
            } else if (abs(lastHop) > g_bfr.leaf_head.distance) {
                msg->lastHopDistance = pack_ieee754_64(-1 * g_bfr.leaf_head.distance);
                fwd = TRUE;
                //log_print(g_log, "handle_bloom: fwding bloom msg to cluster head, distance = %5.5", msg->lastHopDistance);
            } else {
                /* no fwd */
                fwd = FALSE;
            }
        }
    } else {
        /* inter cluster routing is geo-based */
        double distance;

        if (grid_distance(msg->dest_level, msg->dest_clusterId,
                          g_bfr.x, g_bfr.y, &distance) != 0) {
            log_print(g_log, "handle_bloom_msg: failed to calculate distance to %u:%u from %10.2f:%10.2f",
                   msg->dest_level, msg->dest_clusterId, g_bfr.x, g_bfr.y);
            pthread_mutex_unlock(&g_bfr.bfr_lock);
            goto CLEANUP;
        }

        double prev_distance = unpack_ieee754_64(msg->lastHopDistance);
        /* if we are closer and the prev_distance is not less than 0, i.e. its not
         * a hop count */
        if (distance < prev_distance && prev_distance > 0) {
            msg->lastHopDistance = pack_ieee754_64(distance);
            fwd = TRUE;
        }
    }
    #else
    if (msg->ttl > 0) {
        fwd = 1;
        msg->ttl--;
    } else {
        fwd = 0;
    }
    #endif

    pthread_mutex_unlock(&g_bfr.bfr_lock);

    if (fwd) {
        /*log_print(g_log, "handle_bloom_msg: fwding aggregate filter from %u:%u to %u:%u (dist = %5.5f)",
                  msg->origin_level, msg->origin_clusterId, msg->dest_level, msg->dest_clusterId,
                  unpack_ieee754_64(msg->lastHopDistance));*/
        broadcast_bloom_msg(msg);
    }

    CLEANUP:
    tpool_add_job(&_strategy.nonce_pool, (job_fun_t)unregister_nonce, nonce, TPOOL_NO_RV, NULL, NULL);
    free(msg->vector);

    return NULL;
}

static int parse_as_join_msg(struct bfr_msg * msg, struct cluster_join_msg * jmsg)
{
    if (!msg || !jmsg)
        return -1;

    if (msg->hdr.type != MSG_NET_CLUSTER_JOIN) return -1;

    if (msg->hdr.payload_size < CLUSTER_JOIN_MSG_SIZE) {
        log_print(g_log, "parse_as_join_msg: hdr payload size smaller than expected (got %u, expecting >= %u) -- IGNORING!",
               msg->hdr.payload_size, (unsigned)CLUSTER_JOIN_MSG_SIZE);
        return -1;
    }

    struct net_buffer * buffer;
    buffer = net_buffer_createFrom(msg->payload.data,
                                   msg->hdr.payload_size);

    jmsg->cluster_level = net_buffer_getByte(buffer);
    jmsg->cluster_id = net_buffer_getShort(buffer);

    net_buffer_delete(buffer);

    return 0;
}

static void * handle_cluster_join_msg(void * arg)
{
    prctl(PR_SET_NAME, "join_handler", 0, 0, 0);
    struct cluster_join_msg * jmsg = (struct cluster_join_msg *) arg;

    bfrstat_rcvd_join(jmsg);

    /* add the join msg to the list of pending joins */
    pthread_mutex_lock(&pending_joins_lock);
        linked_list_append(pending_joins, jmsg);
    pthread_mutex_unlock(&pending_joins_lock);

    pthread_mutex_lock(&g_bfr.bfr_lock);
        uint32_t cluster_head = g_bfr.leaf_head.nodeId;
        uint32_t my_nodeId = g_bfr.nodeId;
        uint8_t hops = g_bfr.leaf_head.distance + 1;
        unsigned my_cluster = clus_get_clusterId(jmsg->cluster_level);
        struct timespec ts_stale;
        memcpy(&ts_stale, &g_bfr.leaf_head.stale, sizeof(struct timespec));
    pthread_mutex_unlock(&g_bfr.bfr_lock);

    /* check if we should respond */
    if (my_cluster == jmsg->cluster_id) {
        ////log_print(g_log, "handle_cluster_join_msg: rcvd a CLUSTER_JOIN msg for our cluster Id.");

        /* if our cluster head is stale we ignore */
        struct timespec ts_now;
        ts_fromnow(&ts_now);

        if (ts_compare(&ts_now, &ts_stale) && (cluster_head != my_nodeId)) {
            ////log_print(g_log, "handle_cluster_join_msg: our cluster head is stale, do not respond.");
        } else if ((unsigned )jmsg->cluster_level <= g_bfr.num_levels) {
            /* we respond with the cluster head */
            struct cluster_msg response;
            response.cluster_level = jmsg->cluster_level;
            response.cluster_id = jmsg->cluster_id;
            response.cluster_head = cluster_head;
            response.hops = hops;

            /* wait a random period of time, and check for a response */
            long wait = RESPONSE_MAX_WAIT_TIME_MS;
            if (read_rand("/dev/urandom", &wait) != 0) {
                //log_print(g_log, "handle_cluster_join_msg: failed to generate a random!");
            }
            wait = abs(wait % (RESPONSE_MAX_WAIT_TIME_MS));
            ////log_print(g_log, "handle_cluster_join_msg: waiting for %ld ms.", wait);
            msleep(wait);
            int match = 0;

            /* check if anyone responded before us */
            pthread_mutex_lock(&matching_responses_lock);
            int i;
            for (i = 0; i < matching_responses->len; i++) {
                struct cluster_msg * rmsg;
                rmsg = (struct cluster_msg * ) linked_list_get(matching_responses, i);
                if ((rmsg->cluster_id == jmsg->cluster_id) &&
                    (rmsg->cluster_level == jmsg->cluster_level)) {
                    /* received a match! */
                    ////log_print(g_log, "handle_cluster_join_msg: detected another response -- DROPPING.");
                    match = 1;
                    linked_list_remove(matching_responses, i);
                    free(rmsg);
                    break;
                }
            }
            pthread_mutex_unlock(&matching_responses_lock);

            if (!match) {
                ////log_print(g_log, "handle_cluster_join_msg: no response detected -- RESPONDING.");
                broadcast_cluster_msg(&response);
            }
        }
    } else {
        /* we can ignore it */
    }

    /* remove it from the pending list */
    pthread_mutex_lock(&pending_joins_lock);
    int i;
    for (i = 0; i < pending_joins->len; i++) {
        if (linked_list_get(pending_joins, i) == jmsg) {
            ////log_print(g_log, "handle_cluster_join_msg: removed entry from pending join list.");
            linked_list_remove(pending_joins, i);
            break;
        }
    }
    pthread_mutex_unlock(&pending_joins_lock);
    free(jmsg);

    return NULL;
}
