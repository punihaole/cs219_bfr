#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#include "content_name.h"

#include "bfr.h"
#include "bfrd.h"
#include "net_lib.h"

#define BUF_SIZE 256

inline void bfr_did2sockpath(uint32_t daemonId, char * str, int size)
{
    snprintf(str, 256, "/tmp/bfr_%u.sock", daemonId);
}

void bfr_msg_delete(struct bfr_msg * msg)
{
    free(msg->payload.data);
    free(msg);
    msg = NULL;
}

int bfr_sendMsg(struct bfr_msg * msg)
{
    if (!msg) {
        fprintf(stderr, "bfr_sendMsg: msg NULL -- IGNORING.");
        return -1;
    }

    if (!msg->payload.data) {
        fprintf(stderr, "bfr_sendMsg: payload NULL -- IGNORING.");
        return -1;
    }

    int s, len;
    struct sockaddr_un daemon;

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return -1;
    }

    daemon.sun_family = AF_UNIX;
    char sock_path[256];
    bfr_did2sockpath(IP4_to_nodeId(), sock_path, 256);
    strcpy(daemon.sun_path, sock_path);
    len = strlen(daemon.sun_path) + sizeof(daemon.sun_family);
    if (connect(s, (struct sockaddr *)&daemon, len) == -1) {
        perror("connect");
        return -1;
    }

    struct bfr_hdr * hdr = &(msg->hdr);
    struct bfr_payload * pay = &(msg->payload);

    if (send(s, &(hdr->type), sizeof(uint8_t), 0) == -1) {
        perror("send");
        return -1;
    }

    if (send(s, &(hdr->nodeId), sizeof(uint32_t), 0) == -1) {
        perror("send");
        return -1;
    }

    if (send(s, &(hdr->payload_size), sizeof(uint32_t), 0) == -1) {
        perror("send");
        return -1;
    }

    if (send(s, pay->data, sizeof(uint8_t) * hdr->payload_size, 0) == -1) {
        perror("send");
        return -1;
    }

    return 0;
}

int bfr_sendLoc(double x, double y)
{
    int s, len;
    struct sockaddr_un daemon;

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return -1;
    }

    daemon.sun_family = AF_UNIX;
    char sock_path[256];
    bfr_did2sockpath(IP4_to_nodeId(), sock_path, 256);
    strcpy(daemon.sun_path, sock_path);
    len = strlen(daemon.sun_path) + sizeof(daemon.sun_family);
    if (connect(s, (struct sockaddr *)&daemon, len) == -1) {
        perror("connect");
        return -1;
    }

    uint8_t type = MSG_IPC_LOCATION_UPDATE;
    uint32_t nodeId = 0;
    uint32_t payload_size = (2 * sizeof(uint64_t));
    uint64_t x_754 = pack_ieee754_64((long double) x);
    uint64_t y_754 = pack_ieee754_64((long double) y);

    if (send(s, &type, sizeof(uint8_t), 0) == -1) {
        perror("send");
        return -1;
    }

    if (send(s, &nodeId, sizeof(uint32_t), 0) == -1) {
        perror("send");
        return -1;
    }

    if (send(s, &payload_size, sizeof(uint32_t), 0) == -1) {
        perror("send");
        return -1;
    }

    uint8_t buf[2 * sizeof(uint64_t)];
    memcpy(buf, &x_754, sizeof(x_754));
    memcpy(buf + sizeof(x_754), &y_754, sizeof(y_754));

    if (send(s, buf, sizeof(buf), 0) == -1) {
        perror("send");
        return -1;
    }

    close(s);

    return 0;
}

int bfr_sendQry(struct content_name * name,
                  unsigned * orig_level, unsigned * orig_clusterId,
                  unsigned * dest_level, unsigned * dest_clusterId,
                  double * last_hop_distance, int * need_fwd)
{
    if (!name || !dest_level || !dest_clusterId || !need_fwd) {
        fprintf(stderr, "bfr_sendQry: invalid parameter(s) -- IGNORING.");
        return -1;
    }

    int s, len;
    struct sockaddr_un daemon;

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return -1;
    }

    daemon.sun_family = AF_UNIX;
    char sock_path[256];
    bfr_did2sockpath(IP4_to_nodeId(), sock_path, 256);
    strcpy(daemon.sun_path, sock_path);
    len = strlen(daemon.sun_path) + sizeof(daemon.sun_family);
    if (connect(s, (struct sockaddr *)&daemon, len) == -1) {
        perror("connect");
        return -1;
    }

    struct bfr_hdr hdr;
    hdr.type = MSG_IPC_INTEREST_FWD_QUERY;
    hdr.nodeId = 0;
    int nlen = name->len;
    hdr.payload_size = (3 * sizeof(int)) + sizeof(uint64_t) + nlen;

    /* send the header */
    if (send(s, &(hdr.type), sizeof(uint8_t), 0) == -1) {
        perror("send");
        return -1;
    }

    if (send(s, &(hdr.nodeId), sizeof(uint32_t), 0) == -1) {
        perror("send");
        return -1;
    }

    if (send(s, &(hdr.payload_size), sizeof(uint32_t), 0) == -1) {
        perror("send");
        return -1;
    }

    /* we can call send directly on our data since we don't have to worry about
     * byte order (domain socket)
     */
    /* send the interest query msg */
    if (send(s, &name->len, sizeof(int), 0) == -1) {
        perror("send");
        return -1;
    }

    if (send(s, name->full_name, name->len, 0) == -1) {
        perror("send");
        return -1;
    }

    if (send(s, orig_level, sizeof(unsigned), 0) == -1) {
        perror("send");
        return -1;
    }

    if (send(s, orig_clusterId, sizeof(unsigned), 0) == -1) {
        perror("send");
        return -1;
    }

    if (send(s, dest_level, sizeof(unsigned), 0) == -1) {
        perror("send");
        return -1;
    }

    if (send(s, dest_clusterId, sizeof(unsigned), 0) == -1) {
        perror("send");
        return -1;
    }

    uint64_t dist = pack_ieee754_64(*last_hop_distance);
    if (send(s, &dist, sizeof(uint64_t), 0) == -1) {
        perror("send");
        return -1;
    }

    /* wait for the response */

    if (recv(s, orig_level, sizeof(unsigned), 0) < sizeof(unsigned)) {
        perror("recv");
        return -1;
    }

    if (recv(s, orig_clusterId, sizeof(unsigned), 0) < sizeof(unsigned)) {
        perror("recv");
        return -1;
    }

    if (recv(s, dest_level, sizeof(unsigned), 0) < sizeof(unsigned)) {
        perror("recv");
        return -1;
    }

    if (recv(s, dest_clusterId, sizeof(unsigned), 0) < sizeof(unsigned)) {
        perror("recv");
        return -1;
    }

    uint64_t new_dist_754;
    if (recv(s, &new_dist_754, sizeof(uint64_t), 0) < sizeof(uint64_t)) {
        perror("recv");
        return -1;
    }

    double new_dist = unpack_ieee754_64(new_dist_754);
    *last_hop_distance = new_dist;

    if (recv(s, need_fwd, sizeof(int), 0) < sizeof(int)) {
        perror("recv");
        return -1;
    }

    close(s);

    return 0;
}

int bfr_sendWhere(struct content_name * name,
                    unsigned * orig_level, unsigned * orig_clusterId,
                    unsigned * dest_level, unsigned * dest_clusterId,
                    double * distance)
{
    if (!name || !dest_level || !dest_clusterId || !distance) {
        fprintf(stderr, "bfr_sendWhere: invalid parameter(s) -- IGNORING.");
        return -1;
    }

    int s, len;
    struct sockaddr_un daemon;

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return -1;
    }

    daemon.sun_family = AF_UNIX;
    char sock_path[256];
    bfr_did2sockpath(IP4_to_nodeId(), sock_path, 256);
    strcpy(daemon.sun_path, sock_path);
    len = strlen(daemon.sun_path) + sizeof(daemon.sun_family);
    if (connect(s, (struct sockaddr *)&daemon, len) == -1) {
        perror("connect");
        return -1;
    }

    struct bfr_hdr hdr;
    hdr.type = MSG_IPC_INTEREST_DEST_QUERY;
    hdr.nodeId = 0;
    int nlen = name->len;
    hdr.payload_size = (3 * sizeof(uint32_t)) + nlen;

    /* send the header */
    if (send(s, &(hdr.type), sizeof(uint8_t), 0) == -1) {
        perror("send");
        return -1;
    }

    if (send(s, &(hdr.nodeId), sizeof(uint32_t), 0) == -1) {
        perror("send");
        return -1;
    }

    if (send(s, &(hdr.payload_size), sizeof(uint32_t), 0) == -1) {
        perror("send");
        return -1;
    }

    /* we can call send directly on our data since we don't have to worry about
     * byte order (domain socket)
     */
    /* send the interest query msg */
    if (send(s, &nlen, sizeof(int), 0) == -1) {
        perror("send");
        return -1;
    }

    if (send(s, name->full_name, name->len, 0) == -1) {
        perror("send");
        return -1;
    }

    /* wait for the response */
    if (recv(s, orig_level, sizeof(unsigned), 0) < sizeof(unsigned)) {
        perror("recv");
        return -1;
    }

    if (recv(s, orig_clusterId, sizeof(unsigned), 0) < sizeof(unsigned)) {
        perror("recv");
        return -1;
    }

    if (recv(s, dest_level, sizeof(unsigned), 0) < sizeof(unsigned)) {
        perror("recv");
        return -1;
    }

    if (recv(s, dest_clusterId, sizeof(unsigned), 0) < sizeof(unsigned)) {
        perror("recv");
        return -1;
    }

    uint64_t new_dist_754;
    if (recv(s, &new_dist_754, sizeof(uint64_t), 0) < sizeof(uint64_t)) {
        perror("recv");
        return -1;
    }

    double new_dist = unpack_ieee754_64(new_dist_754);

    *distance = new_dist;

    close(s);

    return 0;
}

int bfr_sendDistance(struct content_name * name, int hops)
{
    if (!name) {
        fprintf(stderr, "sendDistance: invalid parameter(s) -- IGNORING.");
        return -1;
    }

    int s, len;
    struct sockaddr_un daemon;

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return -1;
    }

    daemon.sun_family = AF_UNIX;
    char sock_path[256];
    bfr_did2sockpath(IP4_to_nodeId(), sock_path, 256);
    strcpy(daemon.sun_path, sock_path);
    len = strlen(daemon.sun_path) + sizeof(daemon.sun_family);
    if (connect(s, (struct sockaddr *)&daemon, len) == -1) {
        perror("connect");
        return -1;
    }

    struct bfr_hdr hdr;
    hdr.type = MSG_IPC_DISTANCE_UPDATE;
    hdr.nodeId = 0;
    int nlen = name->len;
    hdr.payload_size = 2 * sizeof(uint32_t) + nlen;

    /* send the header */
    if (send(s, &(hdr.type), sizeof(uint8_t), 0) == -1) {
        perror("send");
        return -1;
    }

    if (send(s, &(hdr.nodeId), sizeof(uint32_t), 0) == -1) {
        perror("send");
        return -1;
    }

    if (send(s, &(hdr.payload_size), sizeof(uint32_t), 0) == -1) {
        perror("send");
        return -1;
    }

    /* we can call send directly on our data since we don't have to worry about
     * byte order (domain socket)
     */

    if (send(s, &name->len, sizeof(uint32_t), 0) == -1) {
        perror("send");
        return -1;
    }

    if (send(s, name->full_name, name->len, 0) == -1) {
        perror("send");
        return -1;
    }

    if (send(s, &hops, sizeof(uint32_t), 0) == -1) {
        perror("send");
        return -1;
    }

    close(s);

    return 0;
}

int bfr_queryDistance(unsigned orig_level, unsigned orig_clusterId,
                      unsigned dest_level, unsigned dest_clusterId,
                      double * distance)
{
    int s, len;
    struct sockaddr_un daemon;

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return -1;
    }

    daemon.sun_family = AF_UNIX;
    char sock_path[256];
    bfr_did2sockpath(IP4_to_nodeId(), sock_path, 256);
    strcpy(daemon.sun_path, sock_path);
    len = strlen(daemon.sun_path) + sizeof(daemon.sun_family);
    if (connect(s, (struct sockaddr *)&daemon, len) == -1) {
        perror("connect");
        return -1;
    }

    struct bfr_hdr hdr;
    hdr.type = MSG_IPC_DISTANCE_QUERY;
    hdr.nodeId = 0;
    hdr.payload_size = (4 * sizeof(unsigned));

    /* send the header */
    if (send(s, &(hdr.type), sizeof(uint8_t), 0) == -1) {
        perror("send");
        return -1;
    }

    if (send(s, &(hdr.nodeId), sizeof(uint32_t), 0) == -1) {
        perror("send");
        return -1;
    }

    if (send(s, &(hdr.payload_size), sizeof(uint32_t), 0) == -1) {
        perror("send");
        return -1;
    }

    /* wait for the response */
    if (send(s, &orig_level, sizeof(unsigned), 0) < sizeof(unsigned)) {
        perror("send");
        return -1;
    }

    if (send(s, &orig_clusterId, sizeof(unsigned), 0) < sizeof(unsigned)) {
        perror("send");
        return -1;
    }

    if (send(s, &dest_level, sizeof(unsigned), 0) < sizeof(unsigned)) {
        perror("send");
        return -1;
    }

    if (send(s, &dest_clusterId, sizeof(unsigned), 0) < sizeof(unsigned)) {
        perror("send");
        return -1;
    }

    uint64_t new_dist_754;
    if (recv(s, &new_dist_754, sizeof(uint64_t), 0) < sizeof(uint64_t)) {
        perror("recv");
        return -1;
    }

    double new_dist = unpack_ieee754_64(new_dist_754);

    *distance = new_dist;

    close(s);

    return 0;
}
