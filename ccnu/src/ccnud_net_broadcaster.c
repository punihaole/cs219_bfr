#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <netinet/in.h>

#include "ccnud_net_broadcaster.h"

#include "ccnud.h"
#include "ccnu_packet.h"
#include "ccnud_constants.h"
#include "ccnud_pit.h"
#include "ccnud_stats.h"

#include "bfr.h"

#include "log.h"
#include "net_buffer.h"
#include "net_lib.h"
#include "synch_queue.h"
#include "ts.h"

extern struct log * g_log;
static int _bcast_sock;
static struct sockaddr_in _addr;

int ccnudnb_init()
{
    _bcast_sock = broadcast_socket();
    broadcast_addr(&_addr, LISTEN_PORT);
    return 0;
}

int ccnudnb_express_interest(struct content_name * name, struct content_obj ** content_ptr,
                             int use_opt, struct ccnudnb_options * opt)
{
    if (!name || !content_ptr)
        return -1;

    int rv = -1;

    /* We need to hook into our routing daemon and use the sendWhere
     * query to figure out the dest_level, dest_clusterId, and distance.
     */
    double distance;
    unsigned orig_level_u, orig_clusterId_u;
    unsigned dest_level_u, dest_clusterId_u;
    pthread_mutex_lock(&g_lock);
    int retries = g_interest_attempts;
    int timeout_ms = g_timeout_ms;
    pthread_mutex_unlock(&g_lock);
    int ttl = MAX_TTL;
    int qry = 1;
    PENTRY pe = PIT_INVALID;

    if (use_opt) {
        if ((opt->mode & CCNUDNB_USE_ROUTE) == CCNUDNB_USE_ROUTE) {
            orig_level_u = opt->orig_level_u;
            orig_clusterId_u = opt->orig_clusterId_u;
            dest_level_u = opt->dest_level_u;
            dest_clusterId_u = opt->dest_clusterId_u;
            distance = opt->distance;
            qry = 0;
        }
        if ((opt->mode & CCNUDNB_USE_RETRIES) == CCNUDNB_USE_RETRIES) {
            retries = opt->retries;
        }
        if ((opt->mode & CCNUDNB_USE_TIMEOUT) == CCNUDNB_USE_TIMEOUT) {
            timeout_ms = opt->timeout_ms;
        }
        if ((opt->mode & CCNUDNB_USE_TTL) == CCNUDNB_USE_TTL) {
            ttl = opt->ttl;
        }
    }

    log_print(g_log, "ccnudnb: qry = %d", qry);
    if (qry) {
        log_print(g_log, "ccnudnb: querying bfrd", qry);
        if (bfr_sendWhere(name, &orig_level_u, &orig_clusterId_u,
                            &dest_level_u, &dest_clusterId_u, &distance) < 0) {
            log_print(g_log, "ccnudnb: sendWhere? failed! -- cannot send interest, %s!",
                      name->full_name);
            goto CLEANUP;
        }
        log_print(g_log, "ccnudnb: got route: (%d:%d) -> (%d:%d)", orig_level_u, orig_clusterId_u, dest_level_u, dest_clusterId_u);
    }

	struct ccnu_interest_pkt interest;
	interest.ttl = ttl;
	interest.orig_level = (uint8_t) orig_level_u;
    interest.orig_clusterId = (uint16_t) orig_clusterId_u;
    interest.dest_level = (uint8_t) dest_level_u;
    interest.dest_clusterId = (uint16_t) dest_clusterId_u;
    interest.distance = pack_ieee754_64(distance);
    interest.name = name;

    /* we register the interest so that we can be notified when the data
     * arrives.
     */
    pe = PIT_get_handle(name);
    if (pe < 0) {
        log_print(g_log, "ccnudnb_express_interest: failed to create pit entry");
        goto CLEANUP;
    }
    struct content_obj ** data = NULL;
    if (PIT_point_data(pe, &data) < 0) {
        PIT_close(pe);
        log_print(g_log, "ccnudnb_express_interest: failed to get data ptr");
        goto CLEANUP;
    }

    /* we send the interest, and timeout if it is not fullfilled.
     * retransmit after the timeout up until max attempts.
     */
    int i;
    struct timespec ts;
    for (i = 0; i < retries; i++) {
        if (i > 0) {
            log_print(g_log, "ccnudnb_express_interest: retransmitting interest (%s),...",
                      name->full_name);
        }
        PIT_refresh(pe);
        ccnudnb_fwd_interest(&interest);
        /* now that we registered and sent the interest we wait */
        while (!*data) {
            ts_fromnow(&ts);
            ts_addms(&ts, timeout_ms);
            rv = PIT_wait(pe, &ts);

            if (rv == ETIMEDOUT) {
                break;
            }
            if (*data) {
                /* exit the invariant check loop, timed out or rcvd data */
                goto END;
            }
        }
    }

    END:
    rv = 0;
    if (!*data) {
        log_print(g_log, "ccnudnb_express_interest: rtx interest %d times with no data.",i);
        rv = -1;
        goto CLEANUP;
    } else {
        log_print(g_log, "ccnudnb_express_interest: rcvd data %s", name->full_name);
        *content_ptr = *data;
    }

    CLEANUP:
    if (pe >= 0) {
        PIT_release(pe); /* will unlock our mutex */
    }

    return rv;
}

int ccnudnb_fwd_interest(struct ccnu_interest_pkt * interest)
{
    /* we just forward the thing. Whoever calls this upstream set the params */
    if (!interest || !interest->name) return -1;

    struct net_buffer buf;
    net_buffer_init(CCNU_MAX_PACKET_SIZE, &buf);

    net_buffer_putByte(&buf, PACKET_TYPE_INTEREST);
    net_buffer_putByte(&buf, interest->ttl);
    net_buffer_putByte(&buf, interest->orig_level);
    net_buffer_putShort(&buf, interest->orig_clusterId);
    net_buffer_putByte(&buf, interest->dest_level);
    net_buffer_putShort(&buf, interest->dest_clusterId);
    net_buffer_putLong(&buf, interest->distance);
    net_buffer_putInt(&buf, interest->name->len);
    net_buffer_copyTo(&buf, interest->name->full_name, interest->name->len);

    ccnustat_sent_interest(interest);
    int rv = net_buffer_send(&buf, _bcast_sock, &_addr);

    free(buf.buf);

    return rv;
}

int ccnudnb_fwd_data(struct content_obj * content, int hops_taken)
{
    /* we just forward the thing. Whoever calls this upstream set the params */
    if (!content || !content->name || !content->data) return -1;

    struct net_buffer buf;
    net_buffer_init(CCNU_MAX_PACKET_SIZE, &buf);

    net_buffer_putByte(&buf, PACKET_TYPE_DATA);
    net_buffer_putByte(&buf, hops_taken);
    net_buffer_putInt(&buf, content->publisher);
    net_buffer_putInt(&buf, content->name->len);
    net_buffer_copyTo(&buf, content->name->full_name, content->name->len);
    net_buffer_putInt(&buf, content->timestamp);
    net_buffer_putInt(&buf, content->size);
    net_buffer_copyTo(&buf, content->data, content->size);

	ccnustat_sent_data(content);
    int rv = net_buffer_send(&buf, _bcast_sock, &_addr);

    free(buf.buf);

    return rv;
}
