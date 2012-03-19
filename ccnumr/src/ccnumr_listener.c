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

#include "content_name.h"
#include "hash.h"
#include "ts.h"

#include "grid.h"

#include "cluster.h"
#include "ccnumr.h"
#include "ccnumrd.h"
#include "ccnumr_listener.h"
#include "net_lib.h"
#include "log.h"
#include "distance_table.h"

#define BUF_SIZE 512

struct ccnumr_listener
{
    int sock;
    struct sockaddr_un local;
};

struct ccnumr_listener _listener;

static void * fwd_query_response(void * arg);
static void * dest_query_response(void * arg);

int ccnumr_listener_init()
{
    int len;

    if ((_listener.sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        log_print(g_log, "socket: %s.", strerror(errno));
        return -1;
    }

    memset(&(_listener.local), 0, sizeof(struct sockaddr_un));

    _listener.local.sun_family = AF_UNIX;
    char sock_path[256];
    ccnumr_did2sockpath(g_ccnumr.nodeId, sock_path, 256);
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

void ccnumr_listener_close()
{
    close(_listener.sock);
}

int ccnumr_accept(struct ccnumr_msg * msg)
{
    if (!msg) {
        log_print(g_log, "ccnumr_accept: msg NULL -- IGNORING.");
        return -1;
    }

    if (msg->payload.data) {
        log_print(g_log, "ccnumr_accept: payload not null, need to reassign ptr.");
        free(msg->payload.data);
    }

    int sock2;
    struct sockaddr_un remote;
    socklen_t t = sizeof(struct sockaddr_un);

    if ((sock2 = accept(_listener.sock, (struct sockaddr *) &remote, &t)) == -1) {
        log_print(g_log, "accept: %s.", strerror(errno));
        return -1;
    }

    struct ccnumr_hdr * hdr = &msg->hdr;
    struct ccnumr_payload * pay = &msg->payload;

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

    /* these need a response so we 'fork' off a handler */
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
                log_print(g_log, "ccnumr_accept: Should never be here!");
        }

        pthread_t t;
        int * child_sock_ptr = malloc(sizeof(int));
        *child_sock_ptr = sock2;

        if (pthread_create(&t, NULL, handler, child_sock_ptr) != 0) {
            log_print(g_log, "ccnumr_accept: failed to start CS_SUMMARY_REQ handler - %s",
                   strerror(errno));
            return -1;
        }
        if (pthread_detach(t) != 0) {
            log_print(g_log, "handle_net: failed to detach handler - %s", strerror(errno));
            return -1;
        }

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
            log_print(g_log, "ccnumr_accept: rcvd LOCATION_UPDATE");
            break;
        case MSG_IPC_DISTANCE_UPDATE:
            log_print(g_log, "ccnumr_accept: rcvd DISTANCE_UPDATE");
            break;
        default:
            log_print(g_log, "ccnumr_accept: rcvd msg of type = %d -- IGNORING", hdr->type);
            break;
    }

    close(sock2);

    return 0;
}

void * ccnumr_listener_service(void * arg)
{
    prctl(PR_SET_NAME, "ipc", 0, 0, 0);
    struct listener_args * ipc_args = (struct listener_args * )arg;

    log_print(g_log, "ccnumr_listener_service: listening...");
    struct ccnumr_msg * msg;

	while (1) {
	    msg = (struct ccnumr_msg *) malloc(sizeof(struct ccnumr_msg));
	    if (!msg) {
            log_print(g_log, "malloc: %s. -- trying to stay alive!", strerror(errno));
            sleep(1);
            continue;
	    }
	    msg->payload.data = NULL;

		if (ccnumr_accept(msg) < 0) {
		    log_print(g_log, "ccnumr_listener_service: ccnumr_accept failed -- trying to stay alive!");
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
    prctl(PR_SET_NAME, "fwd_qry_res", 0, 0, 0);
    int sock2;
    memcpy(&sock2, (int * )arg, sizeof(int));
    free(arg);

    log_print(g_log, "ccnumr_accept: rcvd INTEREST_FWD_QUERY");
    int name_len;
    char full_name[MAX_NAME_LENGTH];
    unsigned orig_level;
    unsigned orig_clusterId;
    unsigned dest_level;
    unsigned dest_clusterId;
    uint64_t last_hop_dist;

    if (recv(sock2, &name_len, sizeof(int), 0) < sizeof(int)) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_FWD_QRY;
    }

    if (recv(sock2, full_name, name_len, 0) < sizeof(name_len)) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_FWD_QRY;
    }

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

    log_print(g_log, "ccnumr_accept: INTEREST_FWD_QUERY (%d, %s, %d, %d, %10.2f).",
           name_len, full_name, dest_level, dest_clusterId, dist);

    if (clus_get_clusterId(dest_level) == dest_clusterId) {
        /* this is a intra-cluster message. We use the distance table rather
         * than the distance from center metric */
        myDist = (double)dtab_getHops(full_name);
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
        if (clus_get_clusterHead(orig_level) == g_ccnumr.nodeId) {
            /* we reached the cluster head */
            dest_clusterId = clus_get_clusterHead(dest_level - 1);
            dest_level = orig_level - 1;
        }

        if ((rv = grid_distance(g_ccnumr.num_levels, dest_clusterId,
                                g_ccnumr.x, g_ccnumr.y, &myDist))!= 0) {
            log_print(g_log, "ccnumr_accept: failed to calculate distance, must be invalid parameters.");
        }
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

    int response = 0;
    if ((rv == 0) && (abs(myDist) >= abs(dist))) {
        /* respond: don't forward the interest */
        response = 0;
        log_print(g_log, "ccnumr_accept: responded to interest forward query with NO.");
    } else {
        /* we are closer or we aren't sure */
        response = 1;
        log_print(g_log, "ccnumr_accept: responded to interest forward query with YES.");
    }

    if (send(sock2, &response, sizeof(int), 0) == -1) {
        log_print(g_log, "send: %s", strerror(errno));
        goto END_FWD_QRY;
    }

    END_FWD_QRY:

    pthread_exit(NULL);
}

static void * dest_query_response(void * arg)
{
    prctl(PR_SET_NAME, "dst_qry_res", 0, 0, 0);
    int sock2;
    memcpy(&sock2, (int * )arg, sizeof(int));
    free(arg);

    int name_len;
    char str[MAX_NAME_LENGTH];
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

    pthread_mutex_lock(&g_ccnumr.ccnumr_lock);
        unsigned orig_level = g_ccnumr.num_levels;
        unsigned orig_clusterId = g_ccnumr.clusterIds[orig_level - 1];
        unsigned dest_level;
        unsigned dest_clusterId;
        double distance = INFINITY;

        if (clus_findCluster(name, &dest_level, &dest_clusterId) != 0) {
            log_print(g_log, "dest_query_response: failed to find cluster level/Id for content=%s",
                   name->full_name);
            /* set to defaults */
           dest_level = g_ccnumr.num_levels;
           dest_clusterId = clus_get_clusterId(dest_level);
        } else {
           log_print(g_log, "dest_query_response: found cluster level and Id = (%u,%u) for content=%s",
                   dest_level, dest_clusterId, name->full_name);
        }

        grid_distance(dest_level, dest_clusterId, g_ccnumr.x, g_ccnumr.y, &distance);
    pthread_mutex_unlock(&g_ccnumr.ccnumr_lock);

    log_print(g_log, "dest_query_response: for %s - %u:%u -> %u:%u",
              name->full_name, orig_level, orig_clusterId, dest_level, dest_clusterId);

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
    if (send(sock2, &distance_754, sizeof(uint64_t), 0) == -1) {
        log_print(g_log, "send: %s", strerror(errno));
        goto END_DEST_QRY;
    }

    END_DEST_QRY:

    close(sock2);
    if (name) free(name);
    pthread_exit(NULL);
}
