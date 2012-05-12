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
#include "ccnud_constants.h"
#include "ccnud_cs.h"
#include "ccnud_pit.h"
#include "ccnud_stats.h"
#include "ccnud_net_broadcaster.h"
#include "ccnu_packet.h"
#include "ccnud_net_listener.h"

#include "linked_list.h"
#include "bitmap.h"

#include "log.h"
#include "net_buffer.h"
#include "net_lib.h"
#include "thread_pool.h"

#include "bfr.h"

struct ccnud_net_s {
    thread_pool_t packet_pipeline;
    pthread_mutex_t segments_lock;
    struct linked_list * segments;
    pthread_mutex_t nonce_lock;
    struct bitmap * seen_nonces;
    thread_pool_t nonce_pool;
};

struct ccnud_net_s _net;

static void * handle_interest(struct ccnu_interest_pkt * interest);
static void * handle_data(struct ccnu_data_pkt * data);

int ccnudnl_init(int pipeline_size)
{
    if (tpool_create(&_net.packet_pipeline, pipeline_size) < 0) {
        log_print(g_log, "tpool_create: could not create interest thread pool!");
        return -1;
    }

    pthread_mutex_init(&_net.segments_lock, NULL);
    _net.segments = linked_list_init(NULL);
    pthread_mutex_init(&_net.nonce_lock, NULL);
    _net.seen_nonces = bit_create(65535);

    if (tpool_create(&_net.nonce_pool, 50) < 0) {
        log_print(g_log, "tpool_create: could not create nonce thread pool!");
        return -1;
    }

    return 0;
}

int ccnudnl_close()
{
    tpool_shutdown(&_net.packet_pipeline);
    tpool_shutdown(&_net.nonce_pool);
    return 0;
}

void * ccnudnl_service(void * arg)
{
    prctl(PR_SET_NAME, "ccnud_net", 0, 0, 0);

    log_print(g_log, "ccnudnl_service: listening...");

    int rcvd;
    struct net_buffer buf;
    net_buffer_init(CCNU_MAX_PACKET_SIZE + sizeof(struct ether_header), &buf);
	while (1) {
	    net_buffer_reset(&buf);
        rcvd = recvfrom(g_sockfd, buf.buf, buf.size, 0, NULL, NULL);

        // strip off the ethernet header
        struct ether_header eh;
        net_buffer_copyFrom(&buf, &eh, sizeof(eh));

        /*log_debug(g_log, "ccnfdnl_service: rcvd %d bytes from %02x:%02x:%02x:%02x:%02x:%02x",
                  rcvd,
                  eh.ether_shost[0], eh.ether_shost[1], eh.ether_shost[2],
                  eh.ether_shost[3], eh.ether_shost[4], eh.ether_shost[5]);
        */

        if (rcvd <= 0) {
            log_error(g_log, "ccnfdnl_service: recv failed -- trying to stay alive!");
            sleep(1);
            continue;
        }

        uint8_t type = net_buffer_getByte(&buf);

        if (type == PACKET_TYPE_INTEREST) {
            uint16_t nonce = net_buffer_getShort(&buf);
            #ifdef CCNU_NONCE_DETECT
            pthread_mutex_lock(&_net.nonce_lock);
            int seen = bit_test(_net.seen_nonces, nonce);
            if (!seen) {
                bit_set(_net.seen_nonces, nonce);
            }
            pthread_mutex_unlock(&_net.nonce_lock);
            if (seen){
                log_print(g_log, "ccnudnl_service: dropping interest with duplicate nonce (%u)", nonce);
                continue;
            } else {
                log_print(g_log, "ccnudnl_service: adding interest with nonce (%u)", nonce);
            }
            #endif
            struct ccnu_interest_pkt * interest = malloc(sizeof(struct ccnu_interest_pkt));
            interest->nonce = nonce;
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

            tpool_add_job(&_net.packet_pipeline, (job_fun_t)handle_interest, interest,
                          TPOOL_FREE_ARG | TPOOL_NO_RV, (delete_t)ccnu_interest_destroy, NULL);
        } else if (type == PACKET_TYPE_DATA) {
            struct ccnu_data_pkt * data = malloc(sizeof(struct ccnu_data_pkt));
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

            tpool_add_job(&_net.packet_pipeline, (job_fun_t)handle_data, data,
                          TPOOL_FREE_ARG | TPOOL_NO_RV, free, NULL);
        } else {
            log_print(g_log, "ccnudnl_service: recvd unknown msg type - %u", type);
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

#ifdef CCNU_NONCE_DETECT
static void * unregister_nonce(void * arg)
{
    uint16_t nonce;
    memcpy(&nonce, arg, sizeof(uint16_t));
    free(arg);

    msleep(500);
    pthread_mutex_lock(&_net.nonce_lock);
        bit_clear(_net.seen_nonces, nonce);
    pthread_mutex_unlock(&_net.nonce_lock);

    return NULL;
}
#endif

static void * handle_interest(struct ccnu_interest_pkt * interest)
{
    char proc[256];
    snprintf(proc, 256, "hi%u", g_nodeId);
    prctl(PR_SET_NAME, proc, 0, 0, 0);
    int rv = 0;

    uint16_t * nonce = malloc(sizeof(uint16_t));
    *nonce = interest->nonce;

	ccnustat_rcvd_interest(interest);
    log_debug(g_log, "handle_interest: %s from %u:%u->%u:%u %5.5f",
              interest->name->full_name, interest->orig_level, interest->orig_clusterId,
              interest->dest_level, interest->dest_clusterId,
              unpack_ieee754_64(interest->distance));

    PENTRY pe = PIT_exact_match(interest->name);
    if (pe) {

        /* refresh the pit entry */
        pthread_mutex_lock(&g_lock);
        int timeout = g_timeout_ms;
        pthread_mutex_unlock(&g_lock);

        if (PIT_age(pe) >= timeout) {
            log_debug(g_log, "handle_interest: %s refreshing.", interest->name->full_name);
            ccnudnb_fwd_interest(interest);
        }
        PIT_refresh(pe);
        /* we already saw this interest...drop it */
        PIT_close(pe);

    } else {

        // see if we can fullfill
        struct content_obj * content = CS_get(interest->name);

        if (content) {

            log_debug(g_log, "handle_interest: %s (responded)", interest->name->full_name);
            ccnudnb_fwd_data(content, 1);
            content_obj_destroy(content);

        } else {

            /* see if we can satisfy this interest */
            log_print(g_log, "handle_interest: checking if should fwd: %s", interest->name->full_name);
            /* ask routing daemon if we should forward the interest */
            double last_hop_distance = unpack_ieee754_64(interest->distance);
            int need_fwd = 0;
            unsigned orig_level_u = interest->orig_level;
            unsigned orig_clusterId_u = interest->orig_clusterId;
            unsigned dest_level_u = interest->dest_level;
            unsigned dest_clusterId_u = interest->dest_clusterId;
            if ((rv = bfr_sendQry(interest->name,
                                  &orig_level_u, &orig_clusterId_u,
                                  &dest_level_u, &dest_clusterId_u,
                                  &last_hop_distance, &need_fwd)) < 0) {
                log_print(g_log, "handle_interest: bfr_sendQry failed for name - %s.",
                          interest->name->full_name);
                need_fwd = 0;
            }

            if (need_fwd == 1) {
                interest->ttl--;

                ccnudnb_fwd_interest(interest);
                log_print(g_log, "handle_interest: %s forwarding.",
                          interest->name->full_name);
                /* we fwded the interest, add it to the pit */
                PIT_add_entry(interest->name);

            } else {
                log_print(g_log, "handle_interest: %s dropping.", interest->name->full_name);
                /* drop interest */
            }
        }
    }

#ifdef CCNU_NONCE_DETECT
    tpool_add_job(&_net.nonce_pool, (job_fun_t)unregister_nonce, nonce, TPOOL_NO_RV, NULL, NULL);
#endif
    log_print(g_log, "handle_interest: done");

    return NULL;
}

static void * handle_data(struct ccnu_data_pkt * data)
{
    char proc[256];
    snprintf(proc, 256, "hd%u", g_nodeId);
    prctl(PR_SET_NAME, proc, 0, 0, 0);

    log_print(g_log, "handle_data: name: (%s), publisher: %u, timestamp: %u, size: %u",
              data->name->full_name, data->publisher_id, data->timestamp, data->payload_len);

    /* check if it fulfills a registered interest */
    PENTRY pe = PIT_exact_match(data->name);
    if (!pe) {
        ccnustat_rcvd_data_unsolicited(data);
        log_print(g_log, "%s unsolicited data", data->name->full_name);
        /* unsolicited data */
    } else {
        struct content_obj * obj;
        obj = malloc(sizeof(struct content_obj));
        obj->publisher = data->publisher_id;
        obj->name = data->name;
        obj->timestamp = data->timestamp;
        obj->size = data->payload_len;
        obj->data = malloc(data->payload_len);
        memcpy(obj->data, data->payload, data->payload_len);

        /* update the forwarding table in the routing daemon (if prefix) */
        if (!content_is_segmented(obj->name)) {
            bfr_sendDistance(obj->name, data->hops);
        }

        ccnustat_rcvd_data(data);
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
            log_print(g_log, "%s fulfilling PIT, fwding data", obj->name->full_name);
            /* we matched an interest rcvd over the net */
            ccnudnb_fwd_data(obj, data->hops + 1);
            PIT_release(pe); /* release will unlock the lock */
            content_obj_destroy(obj);
        }
    }

    log_print(g_log, "handle_data: done");

    return NULL;
}

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
