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

#include "ccnfd_net_broadcaster.h"

#include "ccnfd.h"
#include "ccnf_packet.h"
#include "ccnfd_constants.h"
#include "ccnfd_pit.h"

#include "log.h"
#include "net_buffer.h"
#include "net_lib.h"
#include "synch_queue.h"
#include "ts.h"

extern struct log * g_log;
static int _bcast_sock;
static struct sockaddr_in _addr;

int ccnfdnb_init()
{
    _bcast_sock = broadcast_socket();
    broadcast_addr(&_addr, LISTEN_PORT);
    return 0;
}

int ccnfdnb_express_interest(struct content_name * name, struct content_obj ** content_ptr,
                             int use_opt, struct ccnfdnb_options * opt)
{
    if (!name || !content_ptr)
        return -1;

    int rv = -1;

    struct net_buffer buf;
    net_buffer_init(CCNF_MAX_PACKET_SIZE, &buf);
    PENTRY pe = NULL;
    uint8_t packet_type = PACKET_TYPE_INTEREST;

    /* We need to hook into our routing daemon and use the sendWhere
     * query to figure out the dest_level, dest_clusterId, and distance.
     */
    int retries = INTEREST_MAX_ATTEMPTS;
    int timeout_ms = INTEREST_TIMEOUT_MS;
    int ttl = MAX_TTL;

    if (use_opt) {
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

    uint32_t name_len = name->len;

    net_buffer_putByte(&buf, packet_type);
    net_buffer_putByte(&buf, ttl);
    net_buffer_putInt(&buf, name_len);
    net_buffer_copyTo(&buf, name->full_name, name_len);

    /* we register the interest so that we can be notified when the data
     * arrives.
     */
    pe = PIT_get_handle(name);
    if (!pe) {
        log_print(g_log, "ccnfdnb_express_interest: failed to create pit entry");
        goto CLEANUP;
    }

    /* we send the interest, and timeout if it is not fullfilled.
     * retransmit after the timeout up until max attempts.
     */
    int i;
    struct timespec ts;
    for (i = 0; i < retries; i++) {
        if (i > 0) {
            log_print(g_log, "ccnfdnb_express_interest: retransmitting interest (%s),...",
                      name->full_name);
        }

        net_buffer_send(&buf, _bcast_sock, &_addr);
        /* now that we registered and sent the interest we wait */
        while (!*pe->obj) {
            ts_fromnow(&ts);
            ts_addms(&ts, timeout_ms);
            log_print(g_log, "ccnfdnb_express_interest: waiting for response (%s)...",
                      name->full_name);
            rv = pthread_cond_timedwait(pe->cond, pe->mutex, &ts);
            if (rv == ETIMEDOUT || *pe->obj) {
                /* exit the invariant check loop, timed out or rcvd data */
                break;
            }
        }

        if (*pe->obj) break;
    }

    rv = 0;
    if (!*pe->obj) {
        log_print(g_log, "ccnfdnb_express_interest: rtx interest %d times with no data.",i);
        rv = -1;
        goto CLEANUP;
    }

    CLEANUP:
    if (pe) {
        *content_ptr = *pe->obj;
        PIT_release(pe); /* will unlock our mutex */
    }

    free(buf.buf);

    return rv;
}

int ccnfdnb_fwd_interest(struct ccnf_interest_pkt * interest)
{
    /* we just forward the thing. Whoever calls this upstream set the params */
    if (!interest || !interest->name) return -1;

    struct net_buffer buf;
    net_buffer_init(CCNF_MAX_PACKET_SIZE, &buf);

    net_buffer_putByte(&buf, PACKET_TYPE_INTEREST);
    net_buffer_putByte(&buf, interest->ttl);
    net_buffer_putInt(&buf, interest->name->len);
    net_buffer_copyTo(&buf, interest->name->full_name, interest->name->len);

    int rv = net_buffer_send(&buf, _bcast_sock, &_addr);

    free(buf.buf);

    return rv;
}

int ccnfdnb_fwd_data(struct content_obj * content, int hops_taken)
{
    /* we just forward the thing. Whoever calls this upstream set the params */
    if (!content || !content->name || !content->data) return -1;

    struct net_buffer buf;
    net_buffer_init(CCNF_MAX_PACKET_SIZE, &buf);

    net_buffer_putByte(&buf, PACKET_TYPE_DATA);
    net_buffer_putByte(&buf, hops_taken);
    net_buffer_putInt(&buf, content->publisher);
    net_buffer_putInt(&buf, content->name->len);
    net_buffer_copyTo(&buf, content->name->full_name, content->name->len);
    net_buffer_putInt(&buf, content->timestamp);
    net_buffer_putInt(&buf, content->size);
    net_buffer_copyTo(&buf, content->data, content->size);

    int rv = net_buffer_send(&buf, _bcast_sock, &_addr);

    free(buf.buf);

    return rv;
}
