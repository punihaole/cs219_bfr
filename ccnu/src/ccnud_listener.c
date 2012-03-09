/**
 * ccnud_listener.c
 *
 * This module listens to our domain socket for IPCs and packages incoming
 * messages and places them into a synchronized queue. The daemon can then
 * periodically poll the queue for messages and parse them.
 *
 **/

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#include "ccnu.h"
#include "ccnumr.h"
#include "ccnud.h"

#include "ccnud_cs.h"

#include "ccnud_constants.h"
#include "ccnud_listener.h"
#include "ccnud_net_broadcaster.h"

#include "log.h"
#include "net_buffer.h"
#include "synch_queue.h"

struct ccnud_listener {
    int sock;
    struct sockaddr_un local;
};

struct ccnud_listener _listener;

static void * create_summary_response(void * arg);
static void * publish_response(void * arg);
static void * retrieve_response(void * arg);
static void * seq_response(void * arg);

int ccnudl_init()
{
    int len;

    if ((_listener.sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        log_print(g_log, "socket: %s.", strerror(errno));
        return -1;
    }

    memset(&(_listener.local), 0, sizeof(struct sockaddr_un));

    _listener.local.sun_family = AF_UNIX;
    char sock_path[256];
    ccnu_did2sockpath(g_nodeId, sock_path, 256);
    strcpy(_listener.local.sun_path, sock_path);
    unlink(_listener.local.sun_path);

    len = sizeof(struct sockaddr_un);
    if (bind(_listener.sock, (struct sockaddr * ) &(_listener.local), len) == -1) {
        log_print(g_log, "bind: %s.", strerror(errno));
        close(_listener.sock);
        return -1;
    }

    if (listen(_listener.sock, SOCK_QUEUE) == -1) {
        log_print(g_log, "listen: %s.", strerror(errno));
        return -1;
    }

    return 0;
}

int ccnudl_close()
{
    close(_listener.sock);
    return 0;
}

static int ccnudl_accept(struct ccnud_msg * msg)
{
    if (!msg) {
        log_print(g_log, "ccnud_accept: msg ptr NULL -- IGNORING");
        return -1;
    }

    int sock2, n;
    struct sockaddr_un remote;
    socklen_t t = sizeof(struct sockaddr_un);

    if ((sock2 = accept(_listener.sock, (struct sockaddr *) &remote, &t)) == -1) {
        log_print(g_log, "accept: %s.", strerror(errno));
        return -1;
    }

    if ((n = recv(sock2, &msg->type, sizeof(uint8_t), 0)) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        return -1;
    }

    /* see if we should process the message right now */
    if (msg->type == MSG_IPC_CS_SUMMARY_REQ || msg->type == MSG_IPC_PUBLISH ||
        msg->type == MSG_IPC_RETRIEVE || msg->type == MSG_IPC_SEQ_RETRIEVE) {
        void * func = NULL;
        switch (msg->type) {
            case MSG_IPC_CS_SUMMARY_REQ:
                func = create_summary_response;
                break;
            case MSG_IPC_PUBLISH:
                func = publish_response;
                break;
            case MSG_IPC_RETRIEVE:
                func = retrieve_response;
                break;
            case MSG_IPC_SEQ_RETRIEVE:
                func = seq_response;
                break;
        }
        msg->payload = NULL;
        pthread_t t;
        int * child_sock = malloc(sizeof(int));
        *child_sock = sock2;
        if (pthread_create(&t, NULL, func, child_sock) != 0) {
            log_print(g_log, "ccnud_accept: failed to start %d handler - %s", msg->type, strerror(errno));
            return -1;
        }
        if (pthread_detach(t) != 0) {
            log_print(g_log, "handle_net: failed to detach handler - %s", strerror(errno));
            return -1;
        }

        return 0;
    }

    if ((n = recv(sock2, &msg->payload_size, sizeof(uint32_t), 0)) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        return -1;
    }

    msg->payload = (uint8_t * ) malloc(msg->payload_size);
    n = recv(sock2, msg->payload, msg->payload_size, 0);
    if (n < msg->payload_size) {
        log_print(g_log, "recv: got %d bytes, expected %d bytes!", n, msg->payload_size);
        return -1;
    } else if (n < 0) {
        log_print(g_log, "recv: %s.", strerror(errno));
        return -1;
    }

    close(sock2);
    log_print(g_log, "ccnud_accept: recieved a message of type: %d and size: %d", msg->type, n);

    return 0;
}

///Only our listener thread runs in here.
void * ccnudl_service(void * arg)
{
    prctl(PR_SET_NAME, "ccnud_ipc", 0, 0, 0);
    struct listener_args * ipc_args = (struct listener_args * ) arg;
    log_print(g_log, "ccnud_listener_service: listening...");
    struct ccnud_msg * msg;

	while (1) {
	    msg = (struct ccnud_msg *) malloc(sizeof(struct ccnud_msg));
	    if (!msg) {
            log_print(g_log, "ccnud_listener_service: malloc: %s.", strerror(errno));
            log_print(g_log, "ccnud_listener_service: trying to continue...");
            sleep(1);
            continue;
	    }
		if (ccnudl_accept(msg) < 0) {
		    log_print(g_log, "ccnud_accept failed -- trying to continue...");
		    free(msg);
		    continue;
		}

		if (msg->type == MSG_IPC_CS_SUMMARY_REQ ||
            msg->type == MSG_IPC_RETRIEVE || msg->type == MSG_IPC_PUBLISH) {
            /* already handled, just free the msg */
            free(msg->payload);
            free(msg);
            continue;
		}

        synch_enqueue(ipc_args->queue, (void * )msg);
		/* notify the daemon to wake up and check the queues */
		pthread_mutex_lock(ipc_args->lock);
            pthread_cond_signal(ipc_args->cond);
        pthread_mutex_unlock(ipc_args->lock);
	}

	return NULL;
}

static void * create_summary_response(void * arg)
{
    prctl(PR_SET_NAME, "ccnud_sum", 0, 0, 0);
    log_print(g_log, "create_summary_response: handling CS summary request");
    int sock;
    memcpy(&sock, (int * )arg, sizeof(int));
    free(arg);

    struct bloom * filter;
    int rv = CS_summary(&filter);

    if (!filter || (rv != 0)) {
        goto END_CREATE_SUMMARY_RESP;
    }

    /* finish rcving the summary request packet */
    int payload_size;
    if (recv(sock, &payload_size, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_CREATE_SUMMARY_RESP;
    }

    int ignore;
    if (recv(sock, &ignore, sizeof(int), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_CREATE_SUMMARY_RESP;
    }

    /* response format:
     * vector length : int
     * vector : byte[]
     */

    int n;
    int size = filter->vector->num_words;

    if ((n = send(sock, &size, sizeof(uint32_t), 0)) <= 0) {
        log_print(g_log, "create_summary_response: send - %s", strerror(errno));
    }

    if ((n = send(sock, filter->vector->map, size * sizeof(uint32_t), 0)) <= 0) {
        log_print(g_log, "create_summary_response: send - %s", strerror(errno));
    }

    END_CREATE_SUMMARY_RESP:
    bloom_destroy(filter);

    close(sock);

    pthread_exit(NULL);
}

static void * publish_response(void * arg)
{
    prctl(PR_SET_NAME, "ccnud_pub", 0, 0, 0);
    log_print(g_log, "publish_response: handling publish request");
    int sock;
    memcpy(&sock, (int * )arg, sizeof(int));
    free(arg);

    struct content_obj * content;
    content = (struct content_obj *) malloc(sizeof(struct content_obj));

    /* finish rcving the summary request packet */
    /* structure  of publish msg:
     * publisher : int
     * name_len : int
     * name : char[name_len]
     * timestamp : int
     * size : int
     * data : byte[size]
     */
    uint32_t publisher;
    uint32_t payload_size;
    uint32_t name_len;
    uint8_t name[MAX_NAME_LENGTH];
    uint32_t timestamp;
    uint32_t size;
    uint8_t * data = NULL;

    if (recv(sock, &payload_size, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_PUBLISH_RESP;
    }

    if (recv(sock, &publisher, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_PUBLISH_RESP;
    }

    if (recv(sock, &name_len, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_PUBLISH_RESP;
    }
    if (name_len > MAX_NAME_LENGTH)
        name_len = MAX_NAME_LENGTH - 1;

    if (recv(sock, name, name_len, 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_PUBLISH_RESP;
    }
    name[name_len] = '\0';

    if (recv(sock, &timestamp, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_PUBLISH_RESP;
    }

    if (recv(sock, &size, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_PUBLISH_RESP;
    }

    data = (uint8_t *) malloc(size);
    if (recv(sock, data, size, 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        free(data);
        goto END_PUBLISH_RESP;
    }

    content->name = content_name_create((char * )name);
    content->publisher = publisher;
    content->size = size;
    content->timestamp = timestamp;
    content->data = data;

    int rv = CS_put(content);
    if (send(sock, &rv, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_PUBLISH_RESP;
    }
    log_print(g_log, "Successfully published content:\n");
    log_print(g_log, "     name = %s\n", name);
    log_print(g_log, "     timestamp = %d\n", timestamp);
    log_print(g_log, "     data size = %d\n", size);

    END_PUBLISH_RESP:

    close(sock);
    pthread_exit(NULL);
}

static void * retrieve_response(void * arg)
{
    prctl(PR_SET_NAME, "ccnud_ret", 0, 0, 0);
    int sock;
    memcpy(&sock, (int * )arg, sizeof(int));
    free(arg);

    uint32_t payload_size;
    uint32_t name_len;
    char str[MAX_NAME_LENGTH];
    struct content_name * name = NULL;
    struct content_obj * content = NULL;

    if (recv(sock, &payload_size, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_RETRIEVE_RESP;
    }

    if (recv(sock, &name_len, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_RETRIEVE_RESP;
    }

    if (name_len > MAX_NAME_LENGTH)
        name_len = MAX_NAME_LENGTH - 1;

    if (recv(sock, str, name_len, 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_RETRIEVE_RESP;
    }
    str[name_len] = '\0';

    log_print(g_log, "retrieve_response: handling retrieve request for %s", str);

    name = content_name_create(str);

    /* we check the CS for the content first */
    content = CS_get(name);
    int rv = 0;
    if (!content) {
        log_print(g_log, "retreive_response: CS miss, expressing interest for content %s", str);
        /* we don't have the content, we need to express an intrest for it */
        rv = ccnudnb_express_interest(name, &content, 0, NULL);
    }

    if (send(sock, &rv, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "send: %s.", strerror(errno));
        goto END_RETRIEVE_RESP;
    }

    if (rv != 0) goto END_RETRIEVE_RESP;

    /* return the content */
    uint32_t publisher = content->publisher;
    name_len = content->name->len;
    uint32_t timestamp = content->timestamp;
    uint32_t size = content->size;
    uint8_t * data = content->data;

    if (send(sock, &publisher, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "send: %s.", strerror(errno));
        goto END_RETRIEVE_RESP;
    }

    if (send(sock, &name_len, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "send: %s.", strerror(errno));
        goto END_RETRIEVE_RESP;
    }

    if (send(sock, content->name->full_name, name_len, 0) == -1) {
        log_print(g_log, "send: %s.", strerror(errno));
        goto END_RETRIEVE_RESP;
    }

    if (send(sock, &timestamp, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "send: %s.", strerror(errno));
        goto END_RETRIEVE_RESP;
    }

    if (send(sock, &size, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "send: %s.", strerror(errno));
        goto END_RETRIEVE_RESP;
    }

    if (send(sock, data, size, 0) == -1) {
        log_print(g_log, "send: %s.", strerror(errno));
        goto END_RETRIEVE_RESP;
    }

    END_RETRIEVE_RESP:

    close(sock);

    pthread_exit(NULL);
}

static void * seq_response(void * arg)
{
    prctl(PR_SET_NAME, "ccnud_seq", 0, 0, 0);
    int sock;
    memcpy(&sock, (int * )arg, sizeof(int));
    free(arg);

    uint32_t payload_size;
    uint32_t name_len;
    char str[MAX_NAME_LENGTH];
    struct content_name * name = NULL;
    int segments;
    struct content_obj * content = NULL;

    if (recv(sock, &payload_size, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        int temp = -1;
        send(sock, &temp, sizeof(uint32_t), 0);
        goto END_SEQ_RESP;
    }

    if (recv(sock, &name_len, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        int temp = -1;
        send(sock, &temp, sizeof(uint32_t), 0);
        goto END_SEQ_RESP;
    }

    if (name_len > MAX_NAME_LENGTH)
        name_len = MAX_NAME_LENGTH - 1;

    if (recv(sock, str, name_len, 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        int temp = -1;
        send(sock, &temp, sizeof(uint32_t), 0);
        goto END_SEQ_RESP;
    }
    str[name_len] = '\0';

    if (recv(sock, &segments, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        int temp = -1;
        send(sock, &temp, sizeof(uint32_t), 0);
        goto END_SEQ_RESP;
    }

    log_print(g_log, "seq_response: retrieving %d segments with base: %s",
              segments, str);

    name = content_name_create(str);

    int rv = 0;
    /* we check the CS for the content first */
    int i;
    /* tell the broadcaster not to query the strategy layer to lower overhead */
    ccnudnb_opt_t opts;
    opts.mode = CCNUDNB_USE_ROUTE;
    if (ccnumr_sendWhere(name,
                        &opts.orig_level_u, &opts.orig_clusterId_u,
                        &opts.dest_level_u, &opts.dest_clusterId_u,
                        &opts.distance) < 0) {
        log_print(g_log, "ccnudnb: sendWhere? failed! -- cannot send interest, %s!",
                  name->full_name);
        int temp = -1;
        send(sock, &temp, sizeof(uint32_t), 0);
        goto END_SEQ_RESP;
    }

    char comp[MAX_NAME_LENGTH];
    /* we add a dummy component of maximum length to make sure all the segment
     * names willl be valid.
     */
    snprintf(comp, MAX_NAME_LENGTH, "%d", segments);
    if (content_name_appendComponent(name, comp)) {
        int temp = -1;
        send(sock, &temp, sizeof(uint32_t), 0);
        log_print(g_log, "could not append component to name (too long?)");
        goto END_SEQ_RESP;
    }

    for (i = 0; i < segments; i++) {
        content_name_removeComponent(name, name->num_components - 1);
        snprintf(comp, MAX_NAME_LENGTH, "%d", i);
        content_name_appendComponent(name, comp);
        content = CS_get(name);

        if (!content) {
            log_print(g_log, "retreive_response: CS miss, expressing interest for content %s", str);
            /* we don't have the content, we need to express an intrest for it */
            rv = ccnudnb_express_interest(name, &content, 1, &opts);
        }
    }

    if (send(sock, &rv, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "send: %s.", strerror(errno));
        int temp = -1;
        send(sock, &temp, sizeof(uint32_t), 0);
        goto END_SEQ_RESP;
    }

    if (rv != 0) goto END_SEQ_RESP;

    /* return the content */
    uint32_t publisher = content->publisher;
    name_len = content->name->len;
    uint32_t timestamp = content->timestamp;
    uint32_t size = content->size;
    uint8_t * data = content->data;

    if (send(sock, &publisher, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "send: %s.", strerror(errno));
        goto END_SEQ_RESP;
    }

    if (send(sock, &name_len, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "send: %s.", strerror(errno));
        goto END_SEQ_RESP;
    }

    if (send(sock, content->name->full_name, name_len, 0) == -1) {
        log_print(g_log, "send: %s.", strerror(errno));
        goto END_SEQ_RESP;
    }

    if (send(sock, &timestamp, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "send: %s.", strerror(errno));
        goto END_SEQ_RESP;
    }

    if (send(sock, &size, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "send: %s.", strerror(errno));
        goto END_SEQ_RESP;
    }

    if (send(sock, data, size, 0) == -1) {
        log_print(g_log, "send: %s.", strerror(errno));
        goto END_SEQ_RESP;
    }

    END_SEQ_RESP:

    close(sock);

    pthread_exit(NULL);
}

