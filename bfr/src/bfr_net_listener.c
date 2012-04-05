#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "bfr.h"
#include "bfrd.h"
#include "net_lib.h"
#include "net_buffer.h"
#include "bfr_net_listener.h"
#include "synch_queue.h"

#include "log.h"

struct bfr_net_s {
    int in_sock; /* udp sockets */
};

struct bfr_net_s _net;

int bfr_net_listener_init()
{
    /* set up the listening socket */
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

void bfr_net_listener_close()
{
    close(_net.in_sock);
}

void * bfr_net_listener_service(void * arg)
{
    prctl(PR_SET_NAME, "bfr_net", 0, 0, 0);
    struct listener_args * net_args = (struct listener_args * )arg;

    log_print(g_log, "bfr_net_listener_service: listening...");
    struct bfr_msg * msg;

    int rcvd;
    struct sockaddr_in remote_addr;
    struct net_buffer buf;
    net_buffer_init(BFR_MAX_PACKET_SIZE, &buf);
	while (1) {
	    net_buffer_reset(&buf);
	    rcvd = net_buffer_recv(&buf, _net.in_sock, &remote_addr);

	    if ((uint32_t) ntohl(remote_addr.sin_addr.s_addr) == g_bfr.nodeId) {
            /* self msg */
            continue;
        }

        if (rcvd <= 0) {
            log_print(g_log, "ccnudnl_service: recv failed -- trying to stay alive!");
            sleep(1);
            continue;
        }

	    msg = (struct bfr_msg *) malloc(sizeof(struct bfr_msg));
	    msg->hdr.type = net_buffer_getByte(&buf);
	    msg->hdr.nodeId = net_buffer_getInt(&buf);
	    msg->hdr.payload_size = net_buffer_getInt(&buf);
	    msg->payload.data = malloc(msg->hdr.payload_size);
	    net_buffer_copyFrom(&buf, msg->payload.data, msg->hdr.payload_size);

	    /*log_print(g_log, "rcvd msg of type %d from node %u (IP:port = %s:%d)",
               msg->hdr.type, msg->hdr.nodeId, inet_ntoa(remote_addr.sin_addr), ntohs(remote_addr.sin_port));*/
		synch_enqueue(net_args->queue, (void * ) msg);

		/* notify the daemon to wake up and check the queues */
		pthread_mutex_lock(net_args->lock);
            pthread_cond_signal(net_args->cond);
        pthread_mutex_unlock(net_args->lock);
	}

	pthread_exit(NULL);
}
