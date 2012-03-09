#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#include <fcntl.h>

#include "bloom_filter.h"

#include "ccnumr.h"
#include "ccnumrd.h"
#include "cluster.h"
#include "grid.h"
#include "synch_queue.h"

#include "ccnumr_listener.h"
#include "ccnumr_net_listener.h"

#include "strategy.h"

#include "net_lib.h"
#include "net_buffer.h"
#include "linked_list.h"
#include "log.h"

#include "ccnu.h"
#include "distance_table.h"

struct ccnumr g_ccnumr;
struct log * g_log;

int read_rand(const char * dev, long * rand)
{
    int fp = open(dev, O_RDONLY);

    if (!fp) {
        log_print(g_log, "read_rand: could not open %s.", dev);
        return -1;
    }

    if (read(fp, rand, sizeof(long)) != sizeof(long)) {
        log_print(g_log, "read_rand: read failed - %s", strerror(errno));
        close(fp);
        return -1;
    }

    close(fp);
    return 0;
}

int ccnumr_init(int nodeId, unsigned levels, unsigned width, unsigned height,
               double startX, double startY)
{
    int rv;

    if ((rv = grid_init(levels, width, height)) != 0) {
        log_print(g_log, "ccnumr_init: grid failed to initialize!");
        return rv;
    }

    pthread_mutex_init(&g_ccnumr.ccnumr_lock, NULL);
    g_ccnumr.nodeId = nodeId;
    g_ccnumr.num_levels = levels;
    g_ccnumr.clusterIds = (unsigned * ) malloc(sizeof(unsigned) * levels);
    g_ccnumr.leaf_head.nodeId = 0;
    g_ccnumr.is_cluster_head = bit_create(levels);
    g_ccnumr.leaf_cluster.nodes = linked_list_init((delete_t)clus_destroy_node);
    g_ccnumr.leaf_cluster.level = levels;
    g_ccnumr.levels = (struct ccnumr_level * ) malloc(sizeof(struct ccnumr_level) * levels);
    g_ccnumr.x = startX;
    g_ccnumr.y = startY;

    int i;
    for (i = 0; i < levels; i++) {
        g_ccnumr.levels[i].clusters = linked_list_init((delete_t)clus_destroy_cluster);
    }

    /* iteratively calculate our cluster Id on all levels.*/
    for (i = 0; i < levels; i++) {
        g_ccnumr.clusterIds[i] = grid_cluster(i+1, startX, startY);
    }

    if (dtab_init(DEFAULT_DIST_TABLE_SIZE) != 0) {
        log_print(g_log, "ccnumr_init: failed to initalize distance table.");
        return -1;
    }

    if (ccnu_cs_summary(&g_ccnumr.my_node.filter) != 0) {
        log_print(g_log, "ccnumr_init: failed to get cs summary from ccnud.");
        return -1;
    }

    if (!g_ccnumr.my_node.filter) {
        log_print(g_log, "ccnumr_init: failed to get cs summary from ccnud.");
        return -1;
    }
    log_print(g_log, "ccnumr_init: successfully retrieved cs summary from ccnud.");

    if ((g_ccnumr.sock = broadcast_socket()) == -1) {
        log_print(g_log, "ccnumr_init: failed to get broadcast socket!");
        return -1;
	}

    if (broadcast_addr(&g_ccnumr.bcast_addr, LISTEN_PORT) == -1) {
        log_print(g_log, "ccnumr_init: failed to get broadcast addr!");
        return -1;
	}

    log_print(g_log, "ccnumr_init: successful.");

    return 0;
}

void ccnumr_handle_net(struct listener_args * net_args)
{
    struct ccnumr_msg * msg;
    while (synch_len(net_args->queue) > 0) {
        msg = (struct ccnumr_msg * ) synch_dequeue(net_args->queue);

        if (!msg) {
            log_print(g_log, "ccnumr_handle_net: net queue empty.");
            return;
        }

        if (msg->hdr.nodeId == g_ccnumr.nodeId) {
            free(msg->payload.data);
            free(msg);
            continue;
        }

        if (msg->hdr.type == MSG_NET_BLOOMFILTER_UPDATE) {
            log_print(g_log, "handle_net: Rcvd a MSG_NET_BLOOMFILTER_UPDATE, passing to strategy layer.");
            strategy_passMsg(msg);
        } else if (msg->hdr.type == MSG_NET_CLUSTER_JOIN) {
            log_print(g_log, "handle_net: Rcvd a MSG_NET_CLUSTER_JOIN.");
            strategy_passMsg(msg);
        } else if (msg->hdr.type == MSG_NET_CLUSTER_RESPONSE) {
            log_print(g_log, "handle_net: Rcvd a MSG_NET_CLUSTER_RESPONSE.");
            strategy_passMsg(msg);
        } else {
            log_print(g_log, "handle_net: ignoring msg of type %d.", msg->hdr.type);
            free(msg->payload.data);
            free(msg);
        }
    }
}

/* we receive location updates via our domain socket */
void ccnumr_handle_ipc(struct listener_args * ipc_args)
{
    struct ccnumr_msg * msg;
    while (synch_len(ipc_args->queue) > 0) {
        msg = (struct ccnumr_msg * ) synch_dequeue(ipc_args->queue);

        if (!msg) {
            log_print(g_log, "handle_ipc: ipc queue empty.");
            return;
        }

        /*
         * location_msg
         *   +x : uint64_t
         *   +y : uint64_t
         */
        if (msg->hdr.type == MSG_IPC_LOCATION_UPDATE) {
            if (msg->hdr.payload_size != sizeof(struct loc_msg)) {
                if (msg->hdr.payload_size < sizeof(struct loc_msg)) {
                    log_print(g_log, "handle_ipc: hdr payload size smaller than expected -- IGNORING!");
                    return;
                } else {
                    /* larger than expected */
                    log_print(g_log, "handle_ipc: hdr payload size larger than expected, proceeding with caution!");
                }
            }

            uint64_t x_754;
            memcpy(&x_754, msg->payload.data, sizeof(x_754));
            uint64_t y_754;
            memcpy(&y_754, msg->payload.data + sizeof(x_754), sizeof(y_754));

            double x = unpack_ieee754_64(x_754);
            double y = unpack_ieee754_64(y_754);

            pthread_mutex_lock(&g_ccnumr.ccnumr_lock);
                /* need to recompute the cluster Ids on all levels */
                int i;
                for (i = 0; i < g_ccnumr.num_levels; i++) {
                    g_ccnumr.clusterIds[i] = grid_cluster(i + 1, x, y);
                }
                g_ccnumr.x = x;
                g_ccnumr.y = y;
            pthread_mutex_unlock(&g_ccnumr.ccnumr_lock);

            log_print(g_log, "handle_ipc: updated geo-route position (%10.2f, %10.2f).", x, y);
        } else if (msg->hdr.type == MSG_IPC_DISTANCE_UPDATE) {
            char name[MAX_NAME_LENGTH];
            int name_len = 0;
            int hops = 0;

            int offset = 0;
            memcpy(&name_len, msg->payload.data, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            memcpy(name, msg->payload.data+offset, name_len);
            offset += name_len;
            memcpy(&hops, msg->payload.data+offset, sizeof(uint32_t));

            log_print(g_log, "handle_ipc: updating distance table for %s, hops = %d.", name, hops);
            dtab_setHops(name, hops);
        } else {
            log_print(g_log, "handle_ipc: Rcvd an unexpected msg type: %d.", msg->hdr.type);
        }

        free(msg->payload.data);
        free(msg);
        msg = NULL;
    }
}

bool_t amClusterHead(unsigned level)
{
    uint32_t clusterHead = clus_get_clusterHead(level);
    if (clusterHead == g_ccnumr.nodeId)
        return TRUE;
    else return FALSE;
}
