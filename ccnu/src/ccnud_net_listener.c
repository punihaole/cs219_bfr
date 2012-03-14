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
#include <netinet/in.h>

#include "ccnu.h"
#include "ccnud.h"
#include "ccnud_cs.h"
#include "ccnud_pit.h"
#include "ccnud_net_broadcaster.h"
#include "ccnu_packet.h"
#include "ccnud_net_listener.h"

#include "linked_list.h"

#include "log.h"
#include "net_buffer.h"
#include "net_lib.h"
#include "thread_pool.h"

#include "ccnumr.h"

struct ccnud_net_s {
    int in_sock; /* udp socket */
    thread_pool_t packet_pipeline;
    pthread_mutex_t segments_lock;
    struct linked_list * segments;
};

struct ccnud_net_s _net;

static void * handle_interest(struct ccnu_interest_pkt * interest);
static void * handle_data(struct ccnu_data_pkt * data);

int ccnudnl_init(int pipeline_size)
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

int ccnudnl_close()
{
    close(_net.in_sock);
    tpool_shutdown(&_net.packet_pipeline);
    return 0;
}

void * ccnudnl_service(void * arg)
{
    prctl(PR_SET_NAME, "ccnud_net", 0, 0, 0);
    //struct listener_args * net_args = (struct listener_args * )arg;

    log_print(g_log, "ccnudnl_service: listening...");

    int rcvd;
    struct sockaddr_in remote_addr;
    struct net_buffer buf;
    net_buffer_init(MAX_PACKET_SIZE, &buf);
	while (1) {
        rcvd = net_buffer_recv(&buf, _net.in_sock, &remote_addr);

        if ((uint32_t) ntohl(remote_addr.sin_addr.s_addr) == g_nodeId) {
            /* self msg */
            net_buffer_reset(&buf);
            continue;
        }

        if (rcvd <= 0) {
            log_print(g_log, "ccnudnl_service: recv failed -- trying to stay alive!");
            sleep(1);
            net_buffer_reset(&buf);
            continue;
        }

        uint8_t type = net_buffer_getByte(&buf);

        if (type == PACKET_TYPE_INTEREST) {
            struct ccnu_interest_pkt * interest = malloc(sizeof(struct ccnu_interest_pkt));
            interest->ttl = net_buffer_getByte(&buf);
            interest->orig_level = net_buffer_getByte(&buf);
            interest->orig_clusterId = net_buffer_getShort(&buf);
            interest->dest_level = net_buffer_getByte(&buf);
            interest->dest_clusterId = net_buffer_getShort(&buf);
            interest->distance = net_buffer_getLong(&buf);
            uint32_t name_len = net_buffer_getInt(&buf);
            char str[MAX_NAME_LENGTH];
            if (name_len > MAX_NAME_LENGTH)
                name_len = MAX_NAME_LENGTH-1;
            net_buffer_copyFrom(&buf, str, name_len);
            str[name_len] = '\0';
            interest->name = content_name_create(str);
            tpool_add_job(&_net.packet_pipeline, (job_fun_t)handle_interest, interest,TPOOL_FREE_ARG | TPOOL_NO_RV, free, NULL);
        } else if (type == PACKET_TYPE_DATA) {
            struct ccnu_data_pkt * data = malloc(sizeof(struct ccnu_data_pkt));
            data->hops = net_buffer_getByte(&buf);
            data->publisher_id = net_buffer_getInt(&buf);
            int name_len = net_buffer_getInt(&buf);
            char str[MAX_NAME_LENGTH];
            if (name_len > MAX_NAME_LENGTH)
                name_len = MAX_NAME_LENGTH - 1;
            net_buffer_copyFrom(&buf, str, name_len);
            str[name_len] = '\0';
            data->name = content_name_create(str);
            data->timestamp = net_buffer_getInt(&buf);
            data->payload_len = net_buffer_getInt(&buf);
            data->payload = malloc(sizeof(uint8_t) * data->payload_len);
            net_buffer_copyFrom(&buf, data->payload, data->payload_len);
            if (data->publisher_id != g_nodeId) {
                tpool_add_job(&_net.packet_pipeline, (job_fun_t)handle_data, data, TPOOL_FREE_ARG | TPOOL_NO_RV, free, NULL);
            }
        } else {
            log_print(g_log, "ccnudnl_service: recvd unknown msg type - %u", type);
            sleep(1);
        }

        net_buffer_reset(&buf);
	}

	return NULL;
}

#ifdef CCNU_USE_SLIDING_WINDOW
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
#endif

static void * handle_interest(struct ccnu_interest_pkt * interest)
{
    int rv = 0;

    /*log_print(g_log, "handle_interest: %s from %u:%u->%u:%u",
              interest->name->full_name, interest->orig_level, interest->orig_clusterId,
              interest->dest_level, interest->dest_clusterId);*/

    /* check the CS for data to match interest */
    struct content_obj * content = CS_get(interest->name);
    if (content) {
        ccnudnb_fwd_data(content, 1);
    } else {
        /* fwd interest */
        PENTRY pe = PIT_search(interest->name);
        if (pe) {
            /* refresh the pit entry */
            PIT_refresh(pe);
            /* we already saw this interest...drop it */
            goto END;
        } else {
            /* ask routing daemon if we should forward the interest */
            double last_hop_distance = unpack_ieee754_64(interest->distance);
            int need_fwd = 0;
            unsigned orig_level_u = interest->orig_level;
            unsigned orig_clusterId_u = interest->orig_clusterId;
            unsigned dest_level_u = interest->dest_level;
            unsigned dest_clusterId_u = interest->dest_clusterId;
            if ((rv = ccnumr_sendQry(interest->name,
                                    &orig_level_u, &orig_clusterId_u,
                                    &dest_level_u, &dest_clusterId_u,
                                    &last_hop_distance, &need_fwd)) < 0) {
                log_print(g_log, "handle_interest: ccnumr_sendQry failed for name - %s.",
                          interest->name->full_name);
                goto END;
            }

            if (need_fwd) {
                interest->ttl--;
                if ((rv = ccnudnb_fwd_interest(interest)) < 0) {
                    log_print(g_log, "handle_interest: ccnudnb_fwd_interest failed front name - %s",
                              interest->name->full_name);
                    goto END;
                }

                /* we fwded the interest, add it to the pit */
                PIT_add_entry(interest->name);
            } else {
                /* drop interest */
            }
        }
    }

    END:

    return NULL;
}

static void * handle_data(struct ccnu_data_pkt * data)
{
    /*log_print(g_log, "handle_data: name: (%s), publisher: %u, timestamp: %u, size: %u",
              data->name->full_name, data->publisher_id, data->timestamp, data->payload_len);*/

    struct content_obj * obj;
    obj = (struct content_obj *) malloc(sizeof(struct content_obj));

    obj->publisher = data->publisher_id;
    obj->name = data->name;
    obj->timestamp = data->timestamp;
    obj->size = data->payload_len;
    obj->data = data->payload;

	/* update the forwarding table in the routing daemon */
	//ccnumr_sendDistance(obj->name, data->hops);

    int rv;
    if ((rv = CS_put(obj)) != 0) {
        log_print(g_log, "handle_data: failed to put data in CS.");
    }

    /* check if it fulfills a registered interest */
    PENTRY pe = PIT_longest_match(obj->name);
    if (!pe) {
        /* unsolicited data */
        return NULL;
    }

    PIT_refresh(pe);

    if (pe->registered) {
        /* we fulfilled a pit, we need to notify the waiter */
        /* no need to lock the pe, the pit_longest_match did it for us */
        *pe->obj = obj; /* hand them the data and wake them up*/

        #ifdef CCNU_USE_SLIDING_WINDOW
        _segment_q_t * seg = match_segment(obj->name);
        if (seg) {
            /* already locked */
            pthread_mutex_unlock(pe->mutex);
            linked_list_append(seg->rcv_chunks, pe);
            seg->rcv_window++;
            if (seg->rcv_window == seg->max_window) {
                seg->rcv_window = 0;
                pthread_cond_signal(&seg->cond);
            }
            pthread_mutex_unlock(&seg->mutex);
        } else {
            pthread_cond_signal(pe->cond);
            pthread_mutex_unlock(pe->mutex);
        }
        #else
        pthread_cond_signal(pe->cond);
        pthread_mutex_unlock(pe->mutex);
        free(pe);
        #endif

    } else {
        /* we matched an interest rcvd over the net */
        rv = ccnudnb_fwd_data(obj, data->hops + 1);
        PIT_release(pe); /* release will unlock the lock */
    }

    return NULL;
}

#ifdef CCNU_USE_SLIDING_WINDOW
void ccnudnl_reg_segment(_segment_q_t * seg)
{
    pthread_mutex_lock(&_net.segments_lock);
        linked_list_append(_net.segments, seg);
    pthread_mutex_unlock(&_net.segments_lock);
}

void ccnudnl_unreg_segment(_segment_q_t * seg)
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
#endif
