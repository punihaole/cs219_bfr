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
#include "ccnfd.h"
#include "ccnfd_constants.h"
#include "ccnfd_cs.h"
#include "ccnfd_pit.h"
#include "ccnfd_net_broadcaster.h"
#include "ccnf_packet.h"
#include "ccnfd_net_listener.h"

#include "linked_list.h"

#include "log.h"
#include "net_buffer.h"
#include "net_lib.h"
#include "thread_pool.h"

struct ccnfd_net_s {
    int in_sock; /* udp socket */
    thread_pool_t packet_pipeline;
    pthread_mutex_t segments_lock;
    struct linked_list * segments;
};

struct ccnfd_net_s _net;

static void * handle_interest(struct ccnf_interest_pkt * interest);
static void * handle_data(struct ccnf_data_pkt * data);

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

int ccnfdnl_close()
{
    close(_net.in_sock);
    tpool_shutdown(&_net.packet_pipeline);
    return 0;
}

void * ccnfdnl_service(void * arg)
{
    prctl(PR_SET_NAME, "ccnfd_net", 0, 0, 0);
    //struct listener_args * net_args = (struct listener_args * )arg;

    log_print(g_log, "ccnfdnl_service: listening...");

    int rcvd;
    struct sockaddr_in remote_addr;
    struct net_buffer buf;
    net_buffer_init(CCNF_MAX_PACKET_SIZE, &buf);
	while (1) {
	    net_buffer_reset(&buf);
        rcvd = net_buffer_recv(&buf, _net.in_sock, &remote_addr);

        if ((uint32_t) ntohl(remote_addr.sin_addr.s_addr) == g_nodeId) {
            /* self msg */
            continue;
        }

        if (rcvd <= 0) {
            log_print(g_log, "ccnfdnl_service: recv failed -- trying to stay alive!");
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
            tpool_add_job(&_net.packet_pipeline, (job_fun_t)handle_interest, interest,TPOOL_FREE_ARG | TPOOL_NO_RV, free, NULL);
        } else if (type == PACKET_TYPE_DATA) {
            struct ccnf_data_pkt * data = malloc(sizeof(struct ccnf_data_pkt));
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
            log_print(g_log, "ccnfdnl_service: recvd unknown msg type - %u", type);
            sleep(1);
        }
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

static void * handle_interest(struct ccnf_interest_pkt * interest)
{
    char proc[256];
    snprintf(proc, 256, "hi%u", g_nodeId);
    prctl(PR_SET_NAME, proc, 0, 0, 0);

    log_print(g_log, "handle_interest: %s, ttl = %d",
              interest->name->full_name, interest->ttl);

    /* check the CS for data to match interest */
    struct content_obj * content = CS_get(interest->name);
    if (content) {
        log_print(g_log, "handle_interest: %s (responded)", interest->name->full_name);
        ccnfdnb_fwd_data(content, 1);
    } else {
        /* fwd interest */
        PENTRY pe = PIT_search(interest->name);
        if (pe) {
            /* refresh the pit entry */
            if (PIT_age(pe) >= INTEREST_TIMEOUT_MS) {
                log_print(g_log, "handle_interest: expired interest, passing on %s",
                              interest->name->full_name);
                ccnfdnb_fwd_interest(interest);
            }
            PIT_refresh(pe);
            /* we already saw this interest...drop it */
            log_print(g_log, "handle_interest: %s previously seen, refresh PIT.",
                      interest->name->full_name);
            PIT_close(pe);
            goto END;
        } else {
            ccnfdnb_fwd_interest(interest);
            log_print(g_log, "handle_interest: %s forwarding.",
                      interest->name->full_name);

            /* we fwded the interest, add it to the pit */
            PIT_add_entry(interest->name);
        }
    }

    END:
    log_print(g_log, "handle_interest: done");

    return NULL;
}

static void * handle_data(struct ccnf_data_pkt * data)
{
    char proc[256];
    snprintf(proc, 256, "hd%u", g_nodeId);
    prctl(PR_SET_NAME, proc, 0, 0, 0);
    log_print(g_log, "handle_data: name: (%s), publisher: %u, timestamp: %u, size: %u",
              data->name->full_name, data->publisher_id, data->timestamp, data->payload_len);

    struct content_obj * obj;
    obj = (struct content_obj *) malloc(sizeof(struct content_obj));

    obj->publisher = data->publisher_id;
    obj->name = data->name;
    obj->timestamp = data->timestamp;
    obj->size = data->payload_len;
    obj->data = data->payload;

    /* check if it fulfills a registered interest */
    log_print(g_log, "%s searching PIT", obj->name->full_name);
    PENTRY pe = PIT_search(obj->name);
    if (!pe) {
        log_print(g_log, "%s unsolicited data", obj->name->full_name);
        /* unsolicited data */
        goto END;
    }

    log_print(g_log, "%s refreshing PIT", obj->name->full_name);

    CS_put(obj);
    if (!*pe->obj) {
        *pe->obj = obj; /* hand them the data and wake them up*/
    }

    if (pe->registered) {
        PIT_refresh(pe);
        /* we fulfilled a pit, we need to notify the waiter */
        /* no need to lock the pe, the pit_longest_match did it for us */
        #ifdef CCNU_USE_SLIDING_WINDOW
        log_print(g_log, "%s found registered pe", obj->name->full_name);
        _segment_q_t * seg = match_segment(obj->name);
        if (seg) {
            log_print(g_log, "%s is segmented content", obj->name->full_name);
            /* already locked */
            pthread_mutex_unlock(pe->mutex);
            linked_list_append(seg->rcv_chunks, pe);
            seg->rcv_window++;
            if (seg->rcv_window >= *seg->max_window / 2) {
                seg->rcv_window = 0;
                pthread_cond_signal(&seg->cond);
                log_print(g_log, "%s signalling segment", obj->name->full_name);
            }
            pthread_mutex_unlock(&seg->mutex);
        } else {
            log_print(g_log, "%s is a chunk, notifiying expresser thread", obj->name->full_name);
            pthread_cond_signal(pe->cond);
            pthread_mutex_unlock(pe->mutex);
        }
        #else
        pthread_cond_signal(pe->cond);
        pthread_mutex_unlock(pe->mutex);
        #endif

    } else {
        log_print(g_log, "%s fulfilling PIT, fwding data", obj->name->full_name);
        /* we matched an interest rcvd over the net */
        ccnfdnb_fwd_data(obj, data->hops + 1);
        log_print(g_log, "fwded, releasing PIT...");
        PIT_release(pe); /* release will unlock the lock */
        log_print(g_log, "released pit");
    }

    END:

    log_print(g_log, "handle_data: done");

    return NULL;
}

#ifdef CCNU_USE_SLIDING_WINDOW
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
#endif
