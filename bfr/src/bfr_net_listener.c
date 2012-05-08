#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <arpa/inet.h>

#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#include "bfr.h"
#include "bfrd.h"
#include "net_lib.h"
#include "net_buffer.h"
#include "bfr_net_listener.h"
#include "synch_queue.h"

#include "log.h"

int bfr_net_listener_init()
{
    /*
    // set up the listening socket
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
    */

    return 0;
}

void bfr_net_listener_close()
{
}

void * bfr_net_listener_service(void * arg)
{
    prctl(PR_SET_NAME, "bfr_net", 0, 0, 0);
    struct listener_args * net_args = arg;

    log_print(g_log, "bfr_net_listener_service: listening...");
    struct bfr_msg * msg;

    int rcvd;
    struct net_buffer buf;
    net_buffer_init(BFR_MAX_PACKET_SIZE, &buf);
	while (1) {
	    net_buffer_reset(&buf);
        rcvd = recvfrom(g_sockfd, buf.buf, buf.size, 0, NULL, NULL);

        // strip off the ethernet header
        struct ether_header eh;
        net_buffer_copyFrom(&buf, &eh, sizeof(eh));

        log_debug(g_log, "bfr_net_listener_service: rcvd %d bytes from %02x:%02x:%02x:%02x:%02x:%02x",
                  rcvd,
                  eh.ether_shost[0], eh.ether_shost[1], eh.ether_shost[2],
                  eh.ether_shost[3], eh.ether_shost[4], eh.ether_shost[5]);

        if (rcvd <= 0) {
            log_error(g_log, "bfr_net_listener_service: recv failed -- trying to stay alive!");
            sleep(1);
            continue;
        }

	    msg = (struct bfr_msg *) malloc(sizeof(struct bfr_msg));
	    msg->hdr.type = net_buffer_getByte(&buf);
	    msg->hdr.nodeId = net_buffer_getInt(&buf);
	    msg->hdr.payload_size = net_buffer_getInt(&buf);
	    msg->payload.data = malloc(msg->hdr.payload_size);
	    net_buffer_copyFrom(&buf, msg->payload.data, msg->hdr.payload_size);

		synch_enqueue(net_args->queue, (void * ) msg);

		/* notify the daemon to wake up and check the queues */
		pthread_mutex_lock(net_args->lock);
            pthread_cond_signal(net_args->cond);
        pthread_mutex_unlock(net_args->lock);
	}

	pthread_exit(NULL);
}
