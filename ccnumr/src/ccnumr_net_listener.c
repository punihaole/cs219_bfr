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

#include "ccnumr.h"
#include "ccnumrd.h"
#include "net_lib.h"
#include "ccnumr_net_listener.h"
#include "synch_queue.h"

#include "log.h"

struct ccnumr_net_s {
    int in_sock; /* udp sockets */
};

struct ccnumr_net_s _net;

int net_sendall(uint8_t * buff, int * len);

int ccnumr_net_listener_init()
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

void ccnumr_net_listener_close()
{
    close(_net.in_sock);
}

int ccnumr_net_recv(struct ccnumr_msg * msg)
{
    if (!msg) {
        log_print(g_log, "ccnumr_net_recv: msg not initialized -- IGNORING!");
        return -1;
    }

    if (msg->payload.data) {
        log_print(g_log, "ccnumr_net_recv: payload not null, need to reassign ptr.");
    }

    int rcvd;
    struct sockaddr_in remote_addr;
    socklen_t len = sizeof(remote_addr);

    uint8_t buf[MAX_PACKET_SIZE];

    rcvd = recvfrom(_net.in_sock, buf, sizeof(buf),
                    0, (struct sockaddr * ) &remote_addr, &len);

    /* transfer the header from the in buffer */
    int offset = 0;
    uint8_t type = getByte(buf+offset);
    offset += sizeof(uint8_t);
    uint32_t nodeId = getInt(buf+offset);
    offset += sizeof(uint32_t);
    uint32_t size = getInt(buf+offset);
    offset += sizeof(uint32_t);

    log_print(g_log, "ccnumr_net_recv: received %d bytes from %s:%d, msg type:%d.", rcvd,
              inet_ntoa(remote_addr.sin_addr), ntohs(remote_addr.sin_port), type);

    msg->hdr.type = type;
    msg->hdr.nodeId = nodeId;
    msg->hdr.payload_size = size;

    int bytes = size * sizeof(uint8_t);
    msg->payload.data = (uint8_t * ) malloc(bytes);

    /* copy the payload */
    memcpy(msg->payload.data, buf+offset, bytes);

    return 0;
}

void * ccnumr_net_listener_service(void * arg)
{
    prctl(PR_SET_NAME, "net", 0, 0, 0);
    struct listener_args * net_args = (struct listener_args * )arg;

    log_print(g_log, "ccnumr_net_listener_service: listening...");
    struct ccnumr_msg * msg;

	while (1) {
	    msg = (struct ccnumr_msg *) malloc(sizeof(struct ccnumr_msg));
	    msg->payload.data = NULL;

	    if (!msg) {
            log_print(g_log, "malloc: %s. -- trying to stay alive!", strerror(errno));
            sleep(5);
            continue;
	    }
		if (ccnumr_net_recv(msg) < 0) {
		    log_print(g_log, "ccnumr_net_listener_service: ccnumr_net_recv failed -- trying to stay alive!");
		    free(msg);
		    sleep(5);
		    continue;
		}

		synch_enqueue(net_args->queue, (void * ) msg);

		/* notify the daemon to wake up and check the queues */
		pthread_mutex_lock(net_args->lock);
            pthread_cond_signal(net_args->cond);
        pthread_mutex_unlock(net_args->lock);
	}

	pthread_exit(NULL);
}
