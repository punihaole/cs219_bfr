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

#include "ccnumr.h"

struct ccnud_net_s {
    int in_sock; /* udp socket */
};

struct ccnud_net_s _net;

static int handle_interest(struct ccnu_interest_pkt * interest);
static int handle_data(struct ccnu_data_pkt * data);

int ccnudnl_init()
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

    return 0;
}

int ccnudnl_close()
{
    close(_net.in_sock);
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
        if (rcvd <= 0) {
            log_print(g_log, "ccnudnl_service: recv failed -- trying to stay alive!");
            sleep(1);
            net_buffer_reset(&buf);
            continue;
        }

        uint8_t type = net_buffer_getByte(&buf);

        if (type == PACKET_TYPE_INTEREST) {
            struct ccnu_interest_pkt interest;
            interest.ttl = net_buffer_getByte(&buf);
            interest.orig_level = net_buffer_getByte(&buf);
            interest.orig_clusterId = net_buffer_getShort(&buf);
            interest.dest_level = net_buffer_getByte(&buf);
            interest.dest_clusterId = net_buffer_getShort(&buf);
            interest.distance = net_buffer_getLong(&buf);
            uint32_t name_len = net_buffer_getInt(&buf);
            char str[MAX_NAME_LENGTH];
            if (name_len > MAX_NAME_LENGTH)
                name_len = MAX_NAME_LENGTH-1;
            net_buffer_copyFrom(&buf, str, name_len);
            str[name_len] = '\0';
            interest.name = content_name_create(str);
            handle_interest(&interest);
        } else if (type == PACKET_TYPE_DATA) {
            struct ccnu_data_pkt data;
            data.hops = net_buffer_getByte(&buf);
            data.publisher_id = net_buffer_getInt(&buf);
            int name_len = net_buffer_getInt(&buf);
            char str[MAX_NAME_LENGTH];
            if (name_len > MAX_NAME_LENGTH)
                name_len = MAX_NAME_LENGTH - 1;
            net_buffer_copyFrom(&buf, str, name_len);
            str[name_len] = '\0';
            data.name = content_name_create(str);
            data.timestamp = net_buffer_getInt(&buf);
            data.payload_len = net_buffer_getInt(&buf);
            data.payload = malloc(sizeof(uint8_t) * data.payload_len);
            net_buffer_copyFrom(&buf, data.payload, data.payload_len);
            if (data.publisher_id != g_nodeId)
                handle_data(&data);
        } else {
            log_print(g_log, "ccnudnl_service: recvd unknown msg type - %u", type);
            sleep(1);
        }

        net_buffer_reset(&buf);
	}

	return NULL;
}


static int handle_interest(struct ccnu_interest_pkt * interest)
{
    int rv = 0;

    log_print(g_log, "handle_interest: %s from %u:%u->%u:%u",
              interest->name->full_name, interest->orig_level, interest->orig_clusterId,
              interest->dest_level, interest->dest_clusterId);

    /* check the CS for data to match interest */
    struct content_obj * content = CS_get(interest->name);
    if (content) {
        log_print(g_log, "handle_interest: found matching data.");
        ccnudnb_fwd_data(content, 1);
    } else {
        /* fwd interest */
        PENTRY pe = PIT_search(interest->name);
        if (pe) {
            log_print(g_log, "handle_interest: already saw this interest, refreshing PIT.");
            /* refresh the pit entry */
            PIT_refresh(pe);
            /* we already saw this interest...drop it */
            goto END;
        } else {
            log_print(g_log, "handle_interest: new interest.");
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
                log_print(g_log, "handle_interest: fwding interest.");

                /* we fwded the interest, add it to the pit */
                PIT_add_entry(interest->name);
            } else {
                log_print(g_log, "handle_interest: dropping interest.");
            }
        }
    }

    END:

    return rv;
}

static int handle_data(struct ccnu_data_pkt * data)
{
    log_print(g_log, "handle_data: name: (%s), publisher: %u, timestamp: %u, size: %u",
              data->name->full_name, data->publisher_id, data->timestamp, data->payload_len);

    struct content_obj * obj;
    obj = (struct content_obj *) malloc(sizeof(struct content_obj));

    obj->publisher = data->publisher_id;
    obj->name = content_name_create(data->name->full_name);
    obj->timestamp = data->timestamp;
    obj->size = data->payload_len;
    obj->data = data->payload;

	/* update the forwarding table in the routing daemon */
	ccnumr_sendDistance(obj->name, data->hops);

    int rv;
    if ((rv = CS_put(obj)) != 0) {
        log_print(g_log, "handle_data: failed to put data in CS.");
    }

    /* check if it fulfills a registered interest */
    PENTRY pe = PIT_longest_match(obj->name);
    if (!pe) {
        /* unsolicited data */
        return 0;
    }

    if (pe->registered) {
        log_print(g_log, "handle_data: fullfilling pending Interest.");
        /* we fulfilled a pit, we need to notify the waiter */
        /* no need to lock the pe, the pit_longest_match did it for us */
        *pe->obj = obj; /* hand them the data and wake them up*/
        pthread_cond_signal(pe->cond);
        pthread_mutex_unlock(pe->mutex);
        free(pe); /* we're done with the handle */
    } else {
        /* we matched an interest rcvd over the net */
        rv = ccnudnb_fwd_data(obj, data->hops + 1);
        PIT_release(pe); /* release will unlock the lock */
    }

    return rv;
}


