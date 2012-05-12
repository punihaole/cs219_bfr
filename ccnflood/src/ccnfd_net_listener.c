#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/ether.h>

#include "ccnu.h"
#include "ccnfd.h"
#include "ccnfd_constants.h"
#include "ccnfd_cs.h"
#include "ccnfd_pit.h"
#include "ccnfd_stats.h"
#include "ccnfd_net_broadcaster.h"
#include "ccnf_packet.h"
#include "ccnfd_net_listener.h"

#include "linked_list.h"

#include "log.h"
#include "net_buffer.h"
#include "net_lib.h"
#include "thread_pool.h"

struct ccnfd_net_s {
    //int in_sock; /* udp socket */
    thread_pool_t packet_pipeline;
    pthread_mutex_t segments_lock;
    struct linked_list * segments;
};

struct ccnfd_net_s _net;

static void * handle_interest(struct ccnf_interest_pkt * interest);
static void * handle_data(struct ccnf_data_pkt * data);

/*
int ccnfdnl_init(int pipeline_size)
{
    if ((_net.in_sock = broadcast_socket()) == -1) {
        log_print(g_log, "failed to create broadcast socket!");
        return -1;
    }

    struct sockaddr_in local;
    memset(&local, 0, sizeof(struct sockaddr_in));
    local.sin_family = AF_INET;
    local.sin_port = htons(LISTEN_PORT);
    local.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    if (bind(_net.in_sock, (struct sockaddr * ) &(local), sizeof(local)) == -1) {
        log_print(g_log, "bind: %s.", strerror(errno));
        close(_net.in_sock);
        return -1;
    }

    if (tpool_create(&_net.packet_pipeline, pipeline_size) < 0) {
        log_print(g_log, "tpool_create: could not create interest thread pool!");
        return -1;
    }

    pthread_mutex_init(&_net.segments_lock, NULL);
    _net.segments = linked_list_init(NULL);

    return 0;
}
*/

int ccnfdnl_init(int pipeline_size)
{
    /*
    struct ifaddrs * ifa, * p;

    if (getifaddrs(&ifa) != 0) {
        log_print(g_log, "ccnfdnl_init: getifaddrs: %s", strerror(errno));
        return -1;
    }

    faces = 0;
    for (p = ifa; p != NULL; p = p->ifa_next) {
        if (p == NULL) continue;
        if (strcmp(p->ifa_name, "lo") == 0) {
            log_print(g_log, "ccnfdnl_init: skipping lo");
            continue;
        }

        sockfd[faces] = socket(PF_PACKET, SOCK_RAW, htons(CCNF_ETHER_PROTO));
        if (sockfd[faces] < 0) {
            log_print(g_log, "ccnfdnl_init: socket: %s", strerror(errno));
            return -1;
        }

        memset(&eth_addr[faces], '\0', sizeof(eth_addr[faces]));
        eth_addr[faces].sll_protocol = htons(CCNF_ETHER_PROTO);
        eth_addr[faces].sll_ifindex = if_nametoindex(p->ifa_name);

        if (bind(sockfd[faces], (struct sockaddr * ) &eth_addr[faces], sizeof(struct sockaddr)) != 0) {
            log_print(g_log, "ccnfdnl_init: bind(%s): %s", p->ifa_name, strerror(errno));
            return -1;
        }

        struct packet_mreq mr;
        memset (&mr, 0, sizeof (mr));
        mr.mr_ifindex = if_nametoindex(p->ifa_name);
        mr.mr_type = PACKET_MR_PROMISC;
        setsockopt(sockfd[faces], SOL_PACKET,PACKET_ADD_MEMBERSHIP, &mr, sizeof (mr));

        faces++;
    }

    freeifaddrs(ifa);
    */

    if (tpool_create(&_net.packet_pipeline, pipeline_size) < 0) {
        log_critical(g_log, "tpool_create: could not create interest thread pool!");
        return -1;
    }

    pthread_mutex_init(&_net.segments_lock, NULL);
    _net.segments = linked_list_init(NULL);

    return 0;
}

int ccnfdnl_close()
{
    tpool_shutdown(&_net.packet_pipeline);
    return 0;
}

void * ccnfdnl_service(void * arg)
{
    prctl(PR_SET_NAME, "ccnfd_net", 0, 0, 0);
    /*
    struct listener_args * net_args = (struct listener_args * )arg;
    int face = net_args->opt;
    free(net_args);
    log_print(g_log, "ccnfdnl_service(%d): listening on %s...", face, g_face_name[face]);
    */
    log_print(g_log, "ccnfdnl_service: listening...");

    int rcvd;
    struct net_buffer buf;
    net_buffer_init(CCNF_MAX_PACKET_SIZE + sizeof(struct ether_header), &buf);
	while (1) {
	    net_buffer_reset(&buf);
        //rcvd = recvfrom(g_sockfd[face], buf.buf, buf.size, 0, NULL, NULL);
        rcvd = recvfrom(g_sockfd, buf.buf, buf.size, 0, NULL, NULL);

        // strip off the ethernet header
        struct ether_header eh;
        net_buffer_copyFrom(&buf, &eh, sizeof(eh));

        log_debug(g_log, "ccnfdnl_service: rcvd %d bytes from %02x:%02x:%02x:%02x:%02x:%02x",
                  rcvd,
                  eh.ether_shost[0], eh.ether_shost[1], eh.ether_shost[2],
                  eh.ether_shost[3], eh.ether_shost[4], eh.ether_shost[5]);

        if (rcvd <= 0) {
            log_error(g_log, "ccnfdnl_service: recv failed -- trying to stay alive!");
            sleep(1);
            continue;
        }

        uint8_t type = net_buffer_getByte(&buf);

        if (type == PACKET_TYPE_INTEREST) {
            struct ccnf_interest_pkt * interest = malloc(sizeof(struct ccnf_interest_pkt));
            interest->ttl = net_buffer_getByte(&buf);
            uint32_t name_len = net_buffer_getInt(&buf);
            char str[MAX_NAME_LENGTH];
            if (name_len > MAX_NAME_LENGTH)
                name_len = MAX_NAME_LENGTH-1;
            net_buffer_copyFrom(&buf, str, name_len);
            str[name_len] = '\0';
            interest->name = content_name_create(str);
            tpool_add_job(&_net.packet_pipeline, (job_fun_t)handle_interest, interest,
                          TPOOL_FREE_ARG | TPOOL_NO_RV, (delete_t)ccnf_interest_destroy, NULL);

        } else if (type == PACKET_TYPE_DATA) {
            struct ccnf_data_pkt * data = malloc(sizeof(struct ccnf_data_pkt));
            data->hops = net_buffer_getByte(&buf);
            data->publisher_id = net_buffer_getInt(&buf);
            if (data->publisher_id == g_nodeId) {
                free(data);
                continue;
            }
            int name_len = net_buffer_getInt(&buf);
            char str[MAX_NAME_LENGTH];
            if (name_len > MAX_NAME_LENGTH)
                name_len = MAX_NAME_LENGTH - 1;
            net_buffer_copyFrom(&buf, str, name_len);
            str[name_len] = '\0';
            data->name = content_name_create(str);
            data->timestamp = net_buffer_getInt(&buf);
            data->payload_len = net_buffer_getInt(&buf);
            data->payload = malloc(data->payload_len);
            net_buffer_copyFrom(&buf, data->payload, data->payload_len);

            tpool_add_job(&_net.packet_pipeline, (job_fun_t)handle_data,
                          data, TPOOL_FREE_ARG | TPOOL_NO_RV, free, NULL);
        } else {
            log_warn(g_log, "ccnfdnl_service: recvd unknown msg type - %u", type);
            sleep(1);
        }
	}

	return NULL;
}

static _segment_q_t * match_segment(struct content_name * name)
{
    struct content_name * base = content_name_create(name->full_name);
    content_name_removeComponent(base, base->num_components - 1);
    int i = 0;
    int found = 0;
    _segment_q_t * seg = NULL;
    pthread_mutex_lock(&_net.segments_lock);
    for (i = 0; i < _net.segments->len; i++) {
        seg = linked_list_get(_net.segments, i);
        if (strcmp(base->full_name, seg->base->full_name) == 0) {
            found = 1;
            break;
        }
    }
    if (found)
        pthread_mutex_lock(&seg->mutex);
    pthread_mutex_unlock(&_net.segments_lock);
    content_name_delete(base);

    if (found) return seg;
    return NULL;
}

static void * handle_interest(struct ccnf_interest_pkt * interest)
{
    char proc[256];
    snprintf(proc, 256, "hi%u", g_nodeId);
    prctl(PR_SET_NAME, proc, 0, 0, 0);

	ccnfstat_rcvd_interest(interest);
    log_debug(g_log, "handle_interest: %s, ttl = %d",
              interest->name->full_name, interest->ttl);

    PENTRY pe = PIT_exact_match(interest->name);
    if (pe) {

        /* refresh the pit entry */
        pthread_mutex_lock(&g_lock);
        int timeout = g_timeout_ms;
        pthread_mutex_unlock(&g_lock);

        if (PIT_age(pe) >= timeout) {
            log_debug(g_log, "handle_interest: %s refreshing.", interest->name->full_name);
            ccnfdnb_fwd_interest(interest);
        } else {
            log_debug(g_log, "handle_interest: %s dropping.", interest->name->full_name);
        }
        PIT_refresh(pe);
        /* we already saw this interest...drop it */
        PIT_close(pe);

    } else {

        // see if we can fullfill
        struct content_obj * content = CS_get(interest->name);

        if (content) {

            log_debug(g_log, "handle_interest: %s (responded)", interest->name->full_name);
            ccnfdnb_fwd_data(content, 1);
            content_obj_destroy(content);

        } else {

            ccnfdnb_fwd_interest(interest);
            log_debug(g_log, "handle_interest: %s (forwarding)", interest->name->full_name);

            /* we fwded the interest, add it to the pit */
            PIT_add_entry(interest->name);

        }
    }

    log_debug(g_log, "handle_interest: done");

    return NULL;
}

static void * handle_data(struct ccnf_data_pkt * data)
{
    char proc[256];
    snprintf(proc, 256, "hd%u", g_nodeId);
    prctl(PR_SET_NAME, proc, 0, 0, 0);

    log_debug(g_log, "handle_data: name: (%s), publisher: %u, timestamp: %u, size: %u",
              data->name->full_name, data->publisher_id, data->timestamp, data->payload_len);

    /* check if it fulfills a registered interest */
    PENTRY pe = PIT_exact_match(data->name);
    if (!pe) {
        log_debug(g_log, "%s unsolicited data", data->name->full_name);
        /* unsolicited data */
		ccnfstat_rcvd_data_unsolicited(data);
    } else {
        struct content_obj * obj = malloc(sizeof(struct content_obj));
        obj->publisher = data->publisher_id;
        obj->name = data->name;
        obj->timestamp = data->timestamp;
        obj->size = data->payload_len;
        obj->data = malloc(data->payload_len);
        memcpy(obj->data, data->payload, data->payload_len);

		ccnfstat_rcvd_data(data);
        CS_put(obj);
        if (!*pe->obj) {
            *pe->obj = obj; /* hand them the data and wake them up*/
        }

        if (pe->registered) {
            PIT_refresh(pe);
            /* we fulfilled a pit, we need to notify the waiter */
            /* no need to lock the pe, the pit_longest_match did it for us */
            _segment_q_t * seg = match_segment(obj->name);
            if (seg) {
                /* already locked */
                pthread_mutex_unlock(pe->mutex);
                linked_list_append(seg->rcv_chunks, pe);
                seg->rcv_window++;
                if (seg->rcv_window >= *seg->max_window) {
                    log_debug(g_log, "%s filled the window, notifiying expresser thread", obj->name->full_name);
                    seg->rcv_window = 0;
                    pthread_cond_signal(&seg->cond);
                }
                pthread_mutex_unlock(&seg->mutex);
            } else {
                log_debug(g_log, "%s is a chunk, notifiying expresser thread", obj->name->full_name);
                pthread_cond_signal(pe->cond);
                pthread_mutex_unlock(pe->mutex);
                free(pe);
            }

        } else {
            log_debug(g_log, "%s fulfilling PIT, fwding data", obj->name->full_name);
            /* we matched an interest rcvd over the net */
            ccnfdnb_fwd_data(obj, data->hops + 1);
            PIT_release(pe); /* release will unlock the lock */
			content_obj_destroy(obj);
        }
    }

    log_debug(g_log, "handle_data: done");

    return NULL;
}

void ccnfdnl_reg_segment(_segment_q_t * seg)
{
    pthread_mutex_lock(&_net.segments_lock);
        linked_list_append(_net.segments, seg);
    pthread_mutex_unlock(&_net.segments_lock);
}

void ccnfdnl_unreg_segment(_segment_q_t * seg)
{
    pthread_mutex_lock(&_net.segments_lock);
    int i;
    int found = 0;
    for (i = 0; i < _net.segments->len; i++) {
        _segment_q_t * s = linked_list_get(_net.segments, i);
        if (s == seg) {
            found = 1;
            break;
        }
    }

    if (found)
        linked_list_remove(_net.segments, i);
    pthread_mutex_unlock(&_net.segments_lock);
}
