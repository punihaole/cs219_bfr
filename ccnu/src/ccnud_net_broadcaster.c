#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>

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
static struct ether_header eh[MAX_INTERFACES];

int ccnudnb_init()
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
        eh[g_faces].ether_type = htons(CCNU_ETHER_PROTO);

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

int ccnudnb_express_interest(struct content_name * name, struct content_obj ** content_ptr,
                             int use_opt, struct ccnu_options * opt)
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
    PENTRY pe = NULL;

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
	interest.nonce = 0; // set before each tx
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
    if (!pe) {
        log_error(g_log, "ccnudnb_express_interest: failed to create pit entry");
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
        interest.nonce = ccnudnb_gen_nonce();
        ccnudnb_fwd_interest(&interest);
        /* now that we registered and sent the interest we wait */
        while (!*pe->obj) {
            ts_fromnow(&ts);
            ts_addms(&ts, timeout_ms);
            rv = pthread_cond_timedwait(pe->cond, pe->mutex, &ts);

            if (rv == ETIMEDOUT || *pe->obj) {
                break;
            }
        }

        if (*pe->obj) break;

    }

    rv = 0;
    if (!*pe->obj) {
        log_print(g_log, "ccnudnb_express_interest: rtx interest %d times with no data.",i);
        rv = -1;
        goto CLEANUP;
    } else {
        log_print(g_log, "ccnudnb_express_interest: rcvd data %s", name->full_name);
        *content_ptr = *pe->obj;
    }

    CLEANUP:
    if (pe) {
        PIT_release(pe); /* will unlock our mutex */
    }

    return rv;
}

uint16_t ccnudnb_gen_nonce()
{
    char * dev = "/dev/random";
    int fp = open(dev, O_RDONLY);
    uint16_t rand;

    if (!fp) {
        log_error(g_log, "read_rand: could not open %s.", dev);
        return -1;
    }

    if (read(fp, &rand, sizeof(uint16_t)) != sizeof(uint16_t)) {
        log_error(g_log, "read_rand: read failed - %s", strerror(errno));
        close(fp);
        return -1;
    }

    close(fp);
    return rand;
}

int ccnudnb_fwd_interest(struct ccnu_interest_pkt * interest)
{
    /* we just forward the thing. Whoever calls this upstream set the params */
    if (!interest || !interest->name) return -1;

    unsigned char buf[CCNU_MAX_PACKET_SIZE + sizeof(struct ether_header)];
    ccnustat_sent_interest(interest);

    uint16_t nonce = htons(interest->nonce);
    uint8_t ttl = interest->ttl;
    uint8_t orig_level = interest->orig_level;
    uint16_t orig_cluster = htons(interest->orig_clusterId);
    uint8_t dest_level = interest->dest_level;
    uint16_t dest_cluster = htons(interest->dest_clusterId);
    uint64_t distance = interest->distance; /* still host order */
    uint32_t name_len = htonl(interest->name->len);

    int n = sizeof(struct ether_header);
    buf[n++] = PACKET_TYPE_INTEREST;
    memcpy(buf+n, &nonce, sizeof(uint16_t));
    n += sizeof(uint16_t);
    buf[n++] = ttl;
    buf[n++] = orig_level;
    memcpy(buf+n, &orig_cluster, sizeof(uint16_t));
    n += sizeof(uint16_t);
    buf[n++] = dest_level;
    memcpy(buf+n, &dest_cluster, sizeof(uint16_t));
    n += sizeof(uint16_t);
    putLong(buf+n, distance); /* copies into buffer and changes to network order */
    n += sizeof(uint64_t);
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
            log_error(g_log, "ccnudnb_fwd_interest: sendto(%d): %s", i, strerror(errno));
        } else if (sent != n) {
            log_warn(g_log, "ccnudnb_fwd_interest: warning sent %d bytes, expected %d!", sent, n);
        } else {
            log_debug(g_log, "ccnudnb_fwd_interest: sent %d bytes", n);
        }
    }

    return 0;
}

int ccnudnb_fwd_data(struct content_obj * content, int hops_taken)
{
    /* we just forward the thing. Whoever calls this upstream set the params */
    if (!content || !content->name || !content->data) return -1;

    uint32_t publisher = htonl(content->publisher);
    uint32_t name_len = htonl(content->name->len);
    uint32_t timestamp = htonl(content->timestamp);
    uint32_t size = htonl(content->size);

    char buf[CCNU_MAX_PACKET_SIZE + sizeof(struct ether_header)];
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

    ccnustat_sent_data(content);

    log_debug(g_log, "ccnudnb_fwd_data: sending %s with %d bytes of data",
              content->name->full_name, content->size);
    int i;
    for (i = 0; i < g_faces; i++) {
        /* ether header */
        memcpy(buf, &eh[i], sizeof(struct ether_header));

        int expected = MIN_DATA_PKT_SIZE + content->name->len + content->size + sizeof(struct ether_header);
        log_assert(g_log, n == expected, "n(%d) != expected(%d)!", n, expected);

        int sent = sendto(g_sockfd, buf, n, 0, (struct sockaddr *)&g_eth_addr[i], sizeof(g_eth_addr[i]));
        if (sent == -1) {
            log_error(g_log, "ccnudnb_fwd_data: sendto(%d): %s", i, strerror(errno));
        } else if (sent != n) {
            log_warn(g_log, "ccnudnb_fwd_data: warning sent %d bytes, expected %d!", sent, n);
        } else {
            log_debug(g_log, "ccnudnb_fwd_data: sent %d bytes", n);
        }
    }

    return 0;
}
