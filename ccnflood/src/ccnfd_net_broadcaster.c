#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/ether.h>
#include <linux/if_packet.h>
#include <ifaddrs.h>

#include "ccnfd_net_broadcaster.h"

#include "ccnfd.h"
#include "ccnf_packet.h"
#include "ccnfd_constants.h"
#include "ccnfd_pit.h"
#include "ccnfd_stats.h"

#include "log.h"
#include "net_buffer.h"
#include "net_lib.h"
#include "synch_queue.h"
#include "ts.h"

extern struct log * g_log;

static struct ether_header eh[MAX_INTERFACES];
//static struct sockaddr_in _addr;

/*
int ccnfdnb_init()
{
    _bcast_sock = broadcast_socket();
    broadcast_addr(&_addr, LISTEN_PORT);
    return 0;
}
*/

int ccnfdnb_init()
{
    g_faces = 0;
    struct ifaddrs * ifa, * p;

    if (getifaddrs(&ifa) != 0) {
        log_critical(g_log, "net_init: getifaddrs: %s", strerror(errno));
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
            log_critical(g_log, "net_init: ioctl: %s", strerror(errno));
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
        eh[g_faces].ether_type = htons(CCNF_ETHER_PROTO);

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

int ccnfdnb_express_interest(struct content_name * name, struct content_obj ** content_ptr,
                             int use_opt, struct ccnfdnb_options * opt)
{
    if (!name || !content_ptr)
        return -1;

    int rv = -1;

	struct ccnf_interest_pkt interest;
    PENTRY pe = NULL;

    /* We need to hook into our routing daemon and use the sendWhere
     * query to figure out the dest_level, dest_clusterId, and distance.
     */
    pthread_mutex_lock(&g_lock);
    int retries = g_interest_attempts;
    int timeout_ms = g_timeout_ms;
    pthread_mutex_unlock(&g_lock);
    int ttl = MAX_TTL;

    if (use_opt) {
        if ((opt->mode & CCNFDNB_USE_RETRIES) == CCNFDNB_USE_RETRIES) {
            retries = opt->retries;
        }
        if ((opt->mode & CCNFDNB_USE_TIMEOUT) == CCNFDNB_USE_TIMEOUT) {
            timeout_ms = opt->timeout_ms;
        }
        if ((opt->mode & CCNFDNB_USE_TTL) == CCNFDNB_USE_TTL) {
            ttl = opt->ttl;
        }
    }

    interest.ttl = ttl;
	interest.name = name;

    /* we register the interest so that we can be notified when the data
     * arrives.
     */
    pe = PIT_get_handle(name);
    if (!pe) {
        log_error(g_log, "ccnfdnb_express_interest: failed to create pit entry");
        goto CLEANUP;
    }

    /* we send the interest, and timeout if it is not fullfilled.
     * retransmit after the timeout up until max attempts.
     */
    int i;
    struct timespec ts;
    for (i = 1; i <= retries; i++) {
        if (i > 1) {
            log_debug(g_log, "ccnfdnb_express_interest: retransmitting interest (%s),...",
                      name->full_name);
        }
		PIT_refresh(pe);
		ccnfdnb_fwd_interest(&interest);

        /* now that we registered and sent the interest we wait */
        while (!*pe->obj) {
            ts_fromnow(&ts);
            ts_addms(&ts, timeout_ms);
            log_debug(g_log, "ccnfdnb_express_interest: waiting for response (%s)...",
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
        log_debug(g_log, "ccnfdnb_express_interest: rtx interest %d times with no data.", i-1);
        rv = -1;
        goto CLEANUP;
    } else {
        log_debug(g_log, "ccnfdnb_express_interest:rcvd data after %d tries.", i);
		*content_ptr = *pe->obj;
    }

    CLEANUP:
    if (pe) {
        PIT_release(pe); /* will unlock our mutex */
    }

    return rv;
}

int ccnfdnb_fwd_interest(struct ccnf_interest_pkt * interest)
{
    /* we just forward the thing. Whoever calls this upstream set the params */
    if (!interest || !interest->name) return -1;

    char buf[CCNF_MAX_PACKET_SIZE + sizeof(struct ether_header)];
    ccnfstat_sent_interest(interest);

    uint32_t name_len = htonl(interest->name->len);

    int n = sizeof(struct ether_header);
    buf[n++] = PACKET_TYPE_INTEREST;
    buf[n++] = interest->ttl;
    memcpy(buf+n, &name_len, sizeof(uint32_t));
    n += sizeof(uint32_t);
    memcpy(buf+n, interest->name->full_name, interest->name->len);
    n += interest->name->len;

    int i;
    for (i = 0; i < g_faces; i++) {
        /* ether header */
        memcpy(buf, &eh[i], sizeof(struct ether_header));

        int expected = MIN_INTEREST_PKT_SIZE + interest->name->len + sizeof(struct ether_header);
        log_assert(g_log, n == expected, "n(%d) != expected(%d)!", n, expected);

        int sent = sendto(g_sockfd, buf, n, 0, (struct sockaddr *) &g_eth_addr[i], sizeof(g_eth_addr[i]));
        if (sent == -1) {
            log_error(g_log, "ccnfdnb_fwd_interest: sendto(%d): %s", i, strerror(errno));
        } else if (sent != n) {
            log_warn(g_log, "ccnfdnb_fwd_interest: warning sent %d bytes, expected %d!", sent, n);
        }

        log_debug(g_log, "ccnfdnb_fwd_interest: sent %d bytes", sent);
    }

    return 0;
}

int ccnfdnb_fwd_data(struct content_obj * content, int hops_taken)
{
    /* we just forward the thing. Whoever calls this upstream set the params */
    if (!content || !content->name || !content->data) return -1;

    uint32_t publisher = htonl(content->publisher);
    uint32_t name_len = htonl(content->name->len);
    uint32_t timestamp = htonl(content->timestamp);
    uint32_t size = htonl(content->size);

    char buf[CCNF_MAX_PACKET_SIZE + sizeof(struct ether_header)];
    int n = sizeof(struct ether_header);
    buf[n++] = PACKET_TYPE_DATA;
    buf[n++] = hops_taken;
    memcpy(buf+n, &publisher, sizeof(uint32_t));
    n += sizeof(uint32_t);
    memcpy(buf+n, &name_len, sizeof(uint32_t));
    n += sizeof(uint32_t);
    memcpy(buf+n, content->name->full_name, content->name->len);
    n += content->name->len;
    memcpy(buf+n, &timestamp, sizeof(uint32_t));
    n += sizeof(uint32_t);
    memcpy(buf+n, &size, sizeof(uint32_t));
    n += sizeof(uint32_t);
    memcpy(buf+n, content->data, content->size);
    n += content->size;

    ccnfstat_sent_data(content);

    log_debug(g_log, "ccnfdnb_fwd_data: sending %s with %d bytes of data",
              content->name->full_name, content->size);
    int i;
    for (i = 0; i < g_faces; i++) {
        /* ether header */
        memcpy(buf, &eh[i], sizeof(struct ether_header));

        int expected = MIN_DATA_PKT_SIZE + content->name->len + content->size + sizeof(struct ether_header);
        log_assert(g_log, n == expected, "n(%d) != expected(%d)!", n, expected);

        int sent = sendto(g_sockfd, buf, n, 0, (struct sockaddr *)&g_eth_addr[i], sizeof(g_eth_addr[i]));
        if (sent == -1) {
            log_error(g_log, "ccnfdnb_fwd_data: sendto(%d): %s", i, strerror(errno));
        } else if (sent != n) {
            log_warn(g_log, "ccnfdnb_fwd_data: warning sent %d bytes, expected %d!", sent, n);
        }
    }

    return 0;
}
