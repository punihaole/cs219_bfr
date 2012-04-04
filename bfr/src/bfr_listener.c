#include <errno.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#include <math.h>

#include "content.h"
#include "content_name.h"
#include "hash.h"
#include "ts.h"

#include "grid.h"

#include "cluster.h"
#include "bfr.h"
#include "bfrd.h"
#include "bfr_listener.h"
#include "net_lib.h"
#include "log.h"
#include "distance_table.h"
#include "thread_pool.h"

#define BUF_SIZE 512

struct bfr_listener
{
    int sock;
    struct sockaddr_un local;
    thread_pool_t handler_pool;
};

struct bfr_listener _listener;

static void * fwd_query_response(void * arg);
static void * dest_query_response(void * arg);

int bfr_listener_init()
{
    int len;

    if (tpool_create(&_listener.handler_pool, 3) < 0) {
        log_print(g_log, "tpool_create: could not create handler thread pool!");
        return -1;
    }

    if ((_listener.sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        log_print(g_log, "socket: %s.", strerror(errno));
        return -1;
    }

    memset(&(_listener.local), 0, sizeof(struct sockaddr_un));

    _listener.local.sun_family = AF_UNIX;
    char sock_path[256];
    bfr_did2sockpath(g_bfr.nodeId, sock_path, 256);
    strcpy(_listener.local.sun_path, sock_path);
    unlink(_listener.local.sun_path);

    len = sizeof(struct sockaddr_un);
    if (bind(_listener.sock, (struct sockaddr * ) &(_listener.local), len) == -1) {
        log_print(g_log, "bind: %s.", strerror(errno));
        return -1;
    }

    if (listen(_listener.sock, SOCK_QUEUE) == -1) {
        log_print(g_log, "listen: %s.", strerror(errno));
        return -1;
    }

    return 0;
}

void bfr_listener_close()
{
    close(_listener.sock);
}

int bfr_accept(struct bfr_msg * msg)
{
    if (!msg) {
        log_print(g_log, "bfr_accept: msg NULL -- IGNORING.");
        return -1;
    }

    if (msg->payload.data) {
        log_print(g_log, "bfr_accept: payload not null, need to reassign ptr.");
        free(msg->payload.data);
    }

    int sock2;
    struct sockaddr_un remote;
    socklen_t t = sizeof(struct sockaddr_un);

    if ((sock2 = accept(_listener.sock, (struct sockaddr *) &remote, &t)) == -1) {
        log_print(g_log, "accept: %s.", strerror(errno));
        return -1;
    }

    struct bfr_hdr * hdr = &msg->hdr;
    struct bfr_payload * pay = &msg->payload;

    if (recv(sock2, &hdr->type, sizeof(uint8_t), 0) < sizeof(uint8_t)) {
        log_print(g_log, "recv: %s.", strerror(errno));
        return -1;
    }

    if (recv(sock2, &hdr->nodeId, sizeof(uint32_t), 0) < sizeof(uint32_t)) {
        log_print(g_log, "recv: %s.", strerror(errno));
        return -1;
    }

    if (recv(sock2, &hdr->payload_size, sizeof(uint32_t), 0) < sizeof(uint32_t)) {
        log_print(g_log, "recv: %s.", strerror(errno));
        return -1;
    }

    /* these need a response so we schedule a handler from the pool */
    if (hdr->type == MSG_IPC_INTEREST_FWD_QUERY || hdr->type == MSG_IPC_INTEREST_DEST_QUERY) {
        void * handler = NULL;
        switch (hdr->type) {
            case MSG_IPC_INTEREST_FWD_QUERY:
                handler = fwd_query_response;
                break;
            case MSG_IPC_INTEREST_DEST_QUERY:
                handler = dest_query_response;
                break;
            default:
                log_print(g_log, "bfr_accept: Should never be here!");
        }

        int * child_sock = malloc(sizeof(int));
        *child_sock = sock2;

        tpool_add_job(&_listener.handler_pool, handler, child_sock, TPOOL_NO_RV, NULL, NULL);
        /*
        pthread_t t;
        if (pthread_create(&t, NULL, handler, child_sock_ptr) != 0) {
            log_print(g_log, "bfr_accept: failed to start CS_SUMMARY_REQ handler - %s",
                   strerror(errno));
            return -1;
        }
        if (pthread_detach(t) != 0) {
            log_print(g_log, "handle_net: failed to detach handler - %s", strerror(errno));
            return -1;
        }
        */

        return 0;
    }

    int bytes = sizeof(uint8_t) * hdr->payload_size;
    pay->data = (uint8_t * ) malloc(bytes);

    if (recv(sock2, pay->data, bytes, 0) < bytes) {
        log_print(g_log, "recv: %s.", strerror(errno));
        return -1;
    }

    /* copied the bytes over the socket into msg struct */
    switch (hdr->type) {
        case MSG_IPC_LOCATION_UPDATE:
            log_print(g_log, "bfr_accept: rcvd LOCATION_UPDATE");
            break;
        case MSG_IPC_DISTANCE_UPDATE:
            log_print(g_log, "bfr_accept: rcvd DISTANCE_UPDATE");
            break;
        default:
            log_print(g_log, "bfr_accept: rcvd msg of type = %d -- IGNORING", hdr->type);
            break;
    }

    close(sock2);

    return 0;
}

void * bfr_listener_service(void * arg)
{
    prctl(PR_SET_NAME, "ipc", 0, 0, 0);
    struct listener_args * ipc_args = (struct listener_args * )arg;

    log_print(g_log, "bfr_listener_service: listening...");
    struct bfr_msg * msg;

	while (1) {
	    msg = (struct bfr_msg *) malloc(sizeof(struct bfr_msg));
	    if (!msg) {
            log_print(g_log, "malloc: %s. -- trying to stay alive!", strerror(errno));
            sleep(1);
            continue;
	    }
	    msg->payload.data = NULL;

		if (bfr_accept(msg) < 0) {
		    log_print(g_log, "bfr_listener_service: bfr_accept failed -- trying to stay alive!");
		    free(msg);
		    sleep(1);
		    continue;
		}

        if (msg->hdr.type == MSG_IPC_INTEREST_FWD_QUERY || msg->hdr.type == MSG_IPC_INTEREST_DEST_QUERY) {
            /* it's already been handled, just delete the msg */
            free(msg);
            continue;
        }
		synch_enqueue(ipc_args->queue, (void * ) msg);

		/* notify the daemon to wake up and check the queues */
		pthread_mutex_lock(ipc_args->lock);
            pthread_cond_signal(ipc_args->cond);
        pthread_mutex_unlock(ipc_args->lock);
	}

	pthread_exit(NULL);
}

static void * fwd_query_response(void * arg)
{
    int sock2;
    memcpy(&sock2, (int * )arg, sizeof(int));
    free(arg);

    log_print(g_log, "bfr_accept: rcvd INTEREST_FWD_QUERY");
    int name_len;
    char full_name[MAX_NAME_LENGTH + 1];
    unsigned orig_level;
    unsigned orig_clusterId;
    unsigned dest_level;
    unsigned dest_clusterId;
    uint64_t last_hop_dist;

    if (recv(sock2, &name_len, sizeof(int), 0) < sizeof(int)) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_FWD_QRY;
    }

    if (name_len > MAX_NAME_LENGTH)
    	name_len = MAX_NAME_LENGTH;

    if (recv(sock2, full_name, name_len, 0) < sizeof(name_len)) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_FWD_QRY;
    }
    full_name[name_len] = '\0';

    if (recv(sock2, &orig_level, sizeof(unsigned), 0) < sizeof(unsigned)) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_FWD_QRY;
    }

    if (recv(sock2, &orig_clusterId, sizeof(unsigned), 0) < sizeof(unsigned)) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_FWD_QRY;
    }

    if (recv(sock2, &dest_level, sizeof(unsigned), 0) < sizeof(unsigned)) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_FWD_QRY;
    }

    if (recv(sock2, &dest_clusterId, sizeof(unsigned), 0) < sizeof(unsigned)) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_FWD_QRY;
    }

    if (recv(sock2, &last_hop_dist, sizeof(uint64_t), 0) < sizeof(uint64_t)) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_FWD_QRY;
    }

    double dist = unpack_ieee754_64(last_hop_dist);
    double myDist;
    int rv;

    ///@TODO add, Bloom filter checking to do route updating */

    log_print(g_log, "fwd_query_response: INTEREST_FWD_QUERY (%d, %s, %d:%d, %d:%d, %10.2f).",
           name_len, full_name, orig_level, orig_clusterId, dest_level, dest_clusterId, dist);

    pthread_mutex_lock(&g_bfr.bfr_lock);
	double x = g_bfr.x;
   	double y = g_bfr.y;
	pthread_mutex_unlock(&g_bfr.bfr_lock);

    struct content_name * name = content_name_create(full_name);
    if (clus_get_clusterId(dest_level) == dest_clusterId) {
        /* this is a intra-cluster message. We use the distance table rather
         * than the distance from center metric */
        char * prefix = full_name;
        if (content_is_segmented(name)) {
            prefix = content_prefix(name);
            myDist = (double)dtab_getHops(prefix);
            free(prefix);
        } else {
            myDist = (double)dtab_getHops(prefix);
        }

        myDist = (double)dtab_getHops(prefix);

        if (myDist < 0) {
            rv = -1;
            myDist = -1 * MAX_HOPS;
        } else {
            rv = 0;
            myDist *= -1; /* a dist < 0 indicates hop count, not geographic */
        }

    } else {
        /* this is an inter-cluster message. We use the route to center
         * metric. */
        if (clus_get_clusterHead(orig_level) == g_bfr.nodeId) {
            /* we reached the cluster head */
            dest_clusterId = clus_get_clusterHead(dest_level - 1);
            dest_level = orig_level - 1;
        }

        if ((rv = grid_distance(g_bfr.num_levels, dest_clusterId,
                                x, y, &myDist))!= 0) {
            log_print(g_log, "fwd_query_response: failed to calculate distance, must be invalid parameters.");
        }
    }

	/* try and see if we can update the route with a shorter path */
	unsigned up_dest_level;
	unsigned up_dest_clusterId;

	int found_clus = clus_findCluster(name, &up_dest_level, &up_dest_clusterId);
	content_name_delete(name);
	int calc_dist = -1;
	bool_t use_update;
	if (found_clus == 0) {
		double up_distance;
		if (up_dest_clusterId == clus_get_clusterId(up_dest_level)) {
            char * prefix = full_name;
            if (content_is_segmented(name)) {
                prefix = content_prefix(name);
                myDist = (double)dtab_getHops(prefix);
                free(prefix);
            } else {
                myDist = (double)dtab_getHops(prefix);
            }
            up_distance = dtab_getHops(prefix);
            use_update = 1;
		} else {
            calc_dist = grid_distance(up_dest_level, up_dest_clusterId, x, y, &up_distance);
            if ((calc_dist == 0) && ((up_distance < myDist) || (myDist < 0))) {
                myDist = up_distance;
                log_print(g_log, "fwd_query_response: updating cluster level and Id = (%u,%u) for content=%s",
                    up_dest_level, up_dest_clusterId, full_name);
                use_update = TRUE;
            }
		}
	}

	/* fwd if:
     * 1. Inter-cluster and closer
     * 2. They were fwding to their cluster head, and we found the actual dest.
     * 3. Intra-cluster and same cluster and closer
     */
    bool_t intra = (dest_level == orig_level) && (dest_clusterId == orig_clusterId);
    bool_t inter = !intra;

    pthread_mutex_lock(&g_bfr.bfr_lock);
    unsigned my_leaf_cluster = clus_leaf_clusterId();
    pthread_mutex_unlock(&g_bfr.bfr_lock);

    bool_t same_cluster = intra && (my_leaf_cluster == orig_clusterId);

    bool_t closer = abs(myDist) < abs(dist);

    log_print(g_log, "intra = %d", intra);
    log_print(g_log, "inter = %d", inter);
    log_print(g_log, "same_cluster = %d", same_cluster);
    log_print(g_log, "closer = %d", closer);
    log_print(g_log, "myDist = %5.5f, lastHop = %5.5f", myDist, dist);
    log_print(g_log, "use_update = %d", use_update);

    int response = 0;

    if ((inter && closer) || (intra && use_update) || (intra && same_cluster && closer)) {
        /* we are closer or we aren't sure */
        if (use_update) {
            dest_level = up_dest_level;
			dest_clusterId = up_dest_clusterId;
        }
        response = 1;
        log_print(g_log, "fwd_query_response: responded to interest forward query with YES.");
    } else {
        /* respond: don't forward the interest */
        response = 0;
        log_print(g_log, "fwd_query_response: responded to interest forward query with NO.");
    }


    if (send(sock2, &orig_level, sizeof(unsigned), 0) == -1) {
        log_print(g_log, "send: %s.", strerror(errno));
        goto END_FWD_QRY;
    }

    if (send(sock2, &orig_clusterId, sizeof(unsigned), 0) == -1) {
        log_print(g_log, "send: %s.", strerror(errno));
        goto END_FWD_QRY;
    }

    if (send(sock2, &dest_level, sizeof(unsigned), 0) == -1) {
        log_print(g_log, "send: %s", strerror(errno));
        goto END_FWD_QRY;
    }

    if (send(sock2, &dest_clusterId, sizeof(unsigned), 0) == -1) {
        log_print(g_log, "send: %s", strerror(errno));
        goto END_FWD_QRY;
    }

    uint64_t myDist_754 = pack_ieee754_64(myDist);
    if (send(sock2, &myDist_754, sizeof(uint64_t), 0) == -1) {
        log_print(g_log, "send: %s", strerror(errno));
        goto END_FWD_QRY;
    }

    if (send(sock2, &response, sizeof(int), 0) == -1) {
        log_print(g_log, "send: %s", strerror(errno));
        goto END_FWD_QRY;
    }

    END_FWD_QRY:
    close(sock2);
    return NULL;
}

static void * dest_query_response(void * arg)
{
    int sock2;
    memcpy(&sock2, (int * )arg, sizeof(int));
    free(arg);

    int name_len;
    char str[MAX_NAME_LENGTH + 1];
    struct content_name * name = NULL;

    if (recv(sock2, &name_len, sizeof(int), 0) < sizeof(int)) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_DEST_QRY;
    }

    if (name_len > MAX_NAME_LENGTH)
        name_len = MAX_NAME_LENGTH - 1;
    if (recv(sock2, str, name_len, 0) < sizeof(name_len)) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_DEST_QRY;
    }
    str[name_len] = '\0';

    name = content_name_create(str);

    pthread_mutex_lock(&g_bfr.bfr_lock);
        unsigned orig_level = g_bfr.num_levels;
        unsigned orig_clusterId = g_bfr.clusterIds[orig_level - 1];
        unsigned dest_level;
        unsigned dest_clusterId;
        double distance = INFINITY;
		log_print(g_log, "dest_query_response: %s", name->full_name);
        if (clus_findCluster(name, &dest_level, &dest_clusterId) != 0) {
            log_print(g_log, "dest_query_response: failed to find cluster level/Id for content=%s",
                   name->full_name);
            /* set to defaults */
           dest_level = g_bfr.num_levels;
           dest_clusterId = clus_get_clusterId(dest_level);
        } else {
           log_print(g_log, "dest_query_response: found cluster level and Id = (%u,%u) for content=%s",
                   dest_level, dest_clusterId, name->full_name);
        }

        if (dest_clusterId == clus_get_clusterId(dest_level)) {
            char * prefix = name->full_name;
            if (content_is_segmented(name)) {
                prefix = content_prefix(name);
                distance = (double)dtab_getHops(prefix);
                free(prefix);
            } else {
                distance = (double)dtab_getHops(prefix);
            }

            if (distance > 0)
                distance *= -1;
        } else
            grid_distance(dest_level, dest_clusterId, g_bfr.x, g_bfr.y, &distance);
    pthread_mutex_unlock(&g_bfr.bfr_lock);

    /* send the response */
    if (send(sock2, &orig_level, sizeof(unsigned), 0) == -1) {
        log_print(g_log, "send: %s", strerror(errno));
        goto END_DEST_QRY;
    }

    if (send(sock2, &orig_clusterId, sizeof(unsigned), 0) == -1) {
        log_print(g_log, "send: %s", strerror(errno));
        goto END_DEST_QRY;
    }

    if (send(sock2, &dest_level, sizeof(unsigned), 0) == -1) {
        log_print(g_log, "send: %s", strerror(errno));
        goto END_DEST_QRY;
    }

    if (send(sock2, &dest_clusterId, sizeof(unsigned), 0) == -1) {
        log_print(g_log, "send: %s", strerror(errno));
        goto END_DEST_QRY;
    }

    uint64_t distance_754 = pack_ieee754_64(distance);
    log_print(g_log, "dest_query_response: sending distance_754");
    if (send(sock2, &distance_754, sizeof(uint64_t), 0) == -1) {
        log_print(g_log, "send: %s", strerror(errno));
        goto END_DEST_QRY;
    }

    END_DEST_QRY:

    close(sock2);
    if (name) content_name_delete(name);
    return NULL;
}
