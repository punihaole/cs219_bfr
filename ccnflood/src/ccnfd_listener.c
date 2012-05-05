/**
 * ccnfd_listener.c
 *
 * This module listens to our domain socket for IPCs and packages incoming
 * messages and places them into a synchronized queue. The daemon can then
 * periodically poll the queue for messages and parse them.
 *
 **/

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#include "ccnf.h"
#include "ccnfd.h"

#include "ccnfd_cs.h"
#include "ccnfd_pit.h"

#include "ccnfd_constants.h"
#include "ccnfd_listener.h"
#include "ccnfd_net_listener.h"
#include "ccnfd_net_broadcaster.h"

#include "bitmap.h"
#include "log.h"
#include "net_buffer.h"
#include "synch_queue.h"
#include "thread_pool.h"
#include "ts.h"

struct ccnfd_listener {
    int sock;
    struct sockaddr_un local;
    thread_pool_t interest_pipeline;
    int interest_pipe_size;
};

struct segment {
    struct content_name * name;
    ccnfdnb_opt_t * opts;
    int num_chunks;
    struct content_obj * obj;
    int chunk_size;
};

struct chunk {
    int seq_no;
    struct ccnf_interest_pkt intr;
    int retries;
};

struct ccnfd_listener _listener;

static void * create_summary_response(void * arg);
static void * publish_response(void * arg);
static void * seq_publish(void * arg);
static void * retrieve_response(void * arg);
static void * seq_response(void * arg);

int ccnfdl_init(int pipeline_size)
{
    int len;

    _listener.interest_pipe_size = pipeline_size;
    int pool_size = INTEREST_FLOWS;
    //int pool_size = 1;
    if (tpool_create(&_listener.interest_pipeline, pool_size) < 0) {
        log_print(g_log, "tpool_create: could not create interest thread pool!");
        return -1;
    }

    if ((_listener.sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        log_print(g_log, "socket: %s.", strerror(errno));
        return -1;
    }

    memset(&(_listener.local), 0, sizeof(struct sockaddr_un));

    _listener.local.sun_family = AF_UNIX;
    char sock_path[256];
    ccnf_did2sockpath(g_nodeId, sock_path, 256);
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

    log_print(g_log, "interest window: %d", _listener.interest_pipe_size);
    log_print(g_log, "max interest window: %d", MAX_INTEREST_PIPELINE);

    return 0;
}

int ccnfdl_close()
{
    close(_listener.sock);
    tpool_shutdown(&_listener.interest_pipeline);
    return 0;
}

static int ccnfdl_accept(struct ccnfd_msg * msg)
{
    if (!msg) {
        log_print(g_log, "ccnfd_accept: msg ptr NULL -- IGNORING");
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
        msg->type == MSG_IPC_SEQ_PUBLISH || msg->type == MSG_IPC_RETRIEVE ||
        msg->type == MSG_IPC_SEQ_RETRIEVE) {
        void * func = NULL;
        switch (msg->type) {
            case MSG_IPC_CS_SUMMARY_REQ:
                func = create_summary_response;
                break;
            case MSG_IPC_PUBLISH:
                func = publish_response;
                break;
            case MSG_IPC_SEQ_PUBLISH:
                func = seq_publish;
                break;
            case MSG_IPC_RETRIEVE:
                func = retrieve_response;
                break;
            case MSG_IPC_SEQ_RETRIEVE:
                func = seq_response;
                break;
        }
        msg->payload = NULL;
        int * child_sock = malloc(sizeof(int));
        *child_sock = sock2;
        tpool_add_job(&_listener.interest_pipeline, func, child_sock, TPOOL_NO_RV, NULL, NULL);

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
    log_print(g_log, "ccnfd_accept: recieved a message of type: %d and size: %d", msg->type, n);

    return 0;
}

void * ccnfdl_service(void * arg)
{
    prctl(PR_SET_NAME, "ccnfd_ipc", 0, 0, 0);
    struct listener_args * ipc_args = (struct listener_args * ) arg;
    log_print(g_log, "ccnfd_listener_service: listening...");
    struct ccnfd_msg * msg;

	while (1) {
	    msg = (struct ccnfd_msg *) malloc(sizeof(struct ccnfd_msg));
	    if (!msg) {
            log_print(g_log, "ccnfd_listener_service: malloc: %s.", strerror(errno));
            log_print(g_log, "ccnfd_listener_service: trying to continue...");
            sleep(1);
            continue;
	    }
		if (ccnfdl_accept(msg) < 0) {
		    log_print(g_log, "ccnfd_accept failed -- trying to continue...");
		    free(msg);
		    continue;
		}

		if (msg->type == MSG_IPC_CS_SUMMARY_REQ || msg->type == MSG_IPC_PUBLISH ||
        msg->type == MSG_IPC_RETRIEVE || msg->type == MSG_IPC_SEQ_RETRIEVE) {
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
    prctl(PR_SET_NAME, "ccnfd_sum", 0, 0, 0);
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

    return NULL;
}

static void * publish_response(void * arg)
{
    prctl(PR_SET_NAME, "ccnfd_pub", 0, 0, 0);
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
    log_print(g_log, "Successfully published content:");
    log_print(g_log, "\tname = %s", name);
    log_print(g_log, "\ttimestamp = %d", timestamp);
    log_print(g_log, "\tdata size = %d", size);

    END_PUBLISH_RESP:

    close(sock);
    return NULL;
}

static void * seq_publish(void * arg)
{
    prctl(PR_SET_NAME, "ccnfd_pubseq", 0, 0, 0);
    log_print(g_log, "seq_publish: handling publish request");
    int sock;
    memcpy(&sock, (int * )arg, sizeof(int));
    free(arg);

    struct content_obj * index_chunk;
    index_chunk = (struct content_obj *) malloc(sizeof(struct content_obj));

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
    int rv = 0;

    if (recv(sock, &payload_size, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        rv = -1;
        goto END_PUBLISH_RESP;
    }

    if (recv(sock, &publisher, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        rv = -1;
        goto END_PUBLISH_RESP;
    }

    if (recv(sock, &name_len, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        rv = -1;
        goto END_PUBLISH_RESP;
    }
    if (name_len > MAX_NAME_LENGTH)
        name_len = MAX_NAME_LENGTH - 1;

    if (recv(sock, name, name_len, 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        rv = -1;
        goto END_PUBLISH_RESP;
    }
    name[name_len] = '\0';

    if (recv(sock, &timestamp, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        rv = -1;
        goto END_PUBLISH_RESP;
    }

    if (recv(sock, &size, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        rv = -1;
        goto END_PUBLISH_RESP;
    }

    data = (uint8_t *) malloc(size);
    if (recv(sock, data, size, 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        free(data);
        rv = -1;
        goto END_PUBLISH_RESP;
    }

    index_chunk->name = content_name_create((char * )name);
    index_chunk->publisher = publisher;
    index_chunk->size = size;
    index_chunk->timestamp = timestamp;
    index_chunk->data = data;

    int num_chunks = 0;
    if (recv(sock, &num_chunks, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        free(data);
        rv = -1;
        goto END_PUBLISH_RESP;
    }

    struct linked_list * chunks = linked_list_init(NULL);
    int i;
    for (i = 0; i < num_chunks; i++) {
        if (recv(sock, &payload_size, sizeof(uint32_t), 0) == -1) {
            log_print(g_log, "recv: %s.", strerror(errno));
            rv = -1;
            goto END_PUBLISH_RESP;
        }

        if (recv(sock, &publisher, sizeof(uint32_t), 0) == -1) {
            log_print(g_log, "recv: %s.", strerror(errno));
            rv = -1;
            goto END_PUBLISH_RESP;
        }

        if (recv(sock, &name_len, sizeof(uint32_t), 0) == -1) {
            log_print(g_log, "recv: %s.", strerror(errno));
            rv = -1;
            goto END_PUBLISH_RESP;
        }
        if (name_len > MAX_NAME_LENGTH)
            name_len = MAX_NAME_LENGTH - 1;

        if (recv(sock, name, name_len, 0) == -1) {
            log_print(g_log, "recv: %s.", strerror(errno));
            rv = -1;
            goto END_PUBLISH_RESP;
        }
        name[name_len] = '\0';

        if (recv(sock, &timestamp, sizeof(uint32_t), 0) == -1) {
            log_print(g_log, "recv: %s.", strerror(errno));
            rv = -1;
            goto END_PUBLISH_RESP;
        }

        if (recv(sock, &size, sizeof(uint32_t), 0) == -1) {
            log_print(g_log, "recv: %s.", strerror(errno));
            rv = -1;
            goto END_PUBLISH_RESP;
        }

        data = (uint8_t *) malloc(size);
        if (recv(sock, data, size, 0) == -1) {
            log_print(g_log, "recv: %s.", strerror(errno));
            rv = -1;
            free(data);
            goto END_PUBLISH_RESP;
        }

        struct content_obj * chunk = malloc(sizeof(struct content_obj));
        chunk->name = content_name_create((char * )name);
        chunk->publisher = publisher;
        chunk->size = size;
        chunk->timestamp = timestamp;
        chunk->data = data;
        linked_list_append(chunks, chunk);
    }

    CS_putSegment(index_chunk, chunks);
    linked_list_delete(chunks);

    if (send(sock, &rv, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        goto END_PUBLISH_RESP;
    }

    log_print(g_log, "Successfully published segment:");
    log_print(g_log, "\tname = %s", index_chunk->name->full_name);
    log_print(g_log, "\ttimestamp = %d", timestamp);
    log_print(g_log, "\tnum chunks = %d", num_chunks);

    END_PUBLISH_RESP:

    close(sock);
    return NULL;
}

static void * retrieve_response(void * arg)
{
    prctl(PR_SET_NAME, "ccnfd_ret", 0, 0, 0);
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
        rv = ccnfdnb_express_interest(name, &content, 0, NULL);
        content_name_delete(name);
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

    return NULL;
}

//static void * retrieve_chunk_job(void * arg)
//{
//    prctl(PR_SET_NAME, "ret_chnk_job", 0, 0, 0);
//
//    struct segment * seg = (struct segment * ) arg;
//
//    log_print(g_log, "retrieve_chunk_job: trying to retrieve segment %s.", seg->name->full_name);
//    struct content_obj * content = CS_get(seg->name);
//
//    int rv;
//    if (!content) {
//        /* we don't have the content, we need to express an interest for it */
//        rv = ccnfdnb_express_interest(seg->name, &content, 1, seg->opts);
//        int attempts = 0;
//        while (rv != 0) {
//            if (attempts > INTEREST_MAX_PATHS) {
//                log_print(g_log, "seq_response: could not fullfill interest for %s after %d attempts.",
//                          seg->name->full_name, INTEREST_MAX_PATHS);
//                rv = -1;
//                goto END;
//            }
//            log_print(g_log, "seq_response: path failed, trying new path for %s.",
//                      seg->name->full_name);
//            /* try a new route */
//            if (bfr_sendWhere(seg->name,
//                                 &seg->opts->orig_level_u, &seg->opts->orig_clusterId_u,
//                                 &seg->opts->dest_level_u, &seg->opts->dest_clusterId_u,
//                                 &seg->opts->distance) < 0) {
//                log_print(g_log, "seq_response: sendWhere? failed! -- cannot send interest, %s!",
//                          seg->name->full_name);
//                rv = -1;
//                goto END;
//            }
//            rv = ccnfdnb_express_interest(seg->name, &content, 1, seg->opts);
//            attempts++;
//        }
//
//        log_print(g_log, "seq_response: retrieved segment %s.", seg->name->full_name);
//
//    }
//
//    END:
//    content_name_delete(seg->name);
//    free(seg);
//
//    struct chunk * chnk = (struct chunk * ) malloc(sizeof(struct chunk));
//    chnk->obj = content;
//    chnk->seq_no = seg->seq_start;
//
//    return chnk;
//}

static int retrieve_segment(struct segment * seg)
{
    int rv = -1;
    prctl(PR_SET_NAME, "ret_seg", 0, 0, 0);
    log_print(g_log, "retrieve_segment: trying to retrieve segment %s[%d - %d].",
              seg->name->full_name, 0, seg->num_chunks-1);

    pthread_mutex_lock(&g_lock);
    int retries = g_interest_attempts;
    int timeout_ms = g_timeout_ms;
    pthread_mutex_unlock(&g_lock);
    int ttl = MAX_TTL;

    if ((seg->opts->mode & CCNFDNB_USE_RETRIES) == CCNFDNB_USE_RETRIES) {
        retries = seg->opts->retries;
    }
    if ((seg->opts->mode & CCNFDNB_USE_TIMEOUT) == CCNFDNB_USE_TIMEOUT) {
        timeout_ms = seg->opts->timeout_ms;
    }
    if ((seg->opts->mode & CCNFDNB_USE_TTL) == CCNFDNB_USE_TTL) {
        ttl = seg->opts->ttl;
    }

    struct chunk chunk_window[MAX_INTEREST_PIPELINE];
    PENTRY _pit_handles[MAX_INTEREST_PIPELINE];
    int pit_to_chunk[PIT_SIZE];
    memset(&pit_to_chunk, 0, sizeof(pit_to_chunk));
    struct bitmap * window = bit_create(MAX_INTEREST_PIPELINE);
    struct bitmap * missing = bit_create(seg->num_chunks);

    char str[MAX_NAME_LENGTH], comp[MAX_NAME_LENGTH];
    strncpy(str, seg->name->full_name, seg->name->len);

    int rtt_est = timeout_ms;
    int cwnd = 1;
    int ssthresh = DEFAULT_INTEREST_PIPELINE;
    int fullfilled = 0;
    int min_rtt_est = 10;

    int current_chunk = 0;
    cc_state state = SLOW_START;
    int tx;

    _segment_q_t seg_q;
    pthread_mutex_init(&seg_q.mutex, NULL);
    pthread_cond_init(&seg_q.cond, NULL);
    seg_q.rcv_window = 0;
    seg_q.max_window = &cwnd;
    seg_q.rcv_chunks = linked_list_init(NULL);
    seg_q.base = seg->name;
    ccnfdnl_reg_segment(&seg_q);

    int i;
    window->num_bits = cwnd;
    while (!bit_allSet(missing)) {
        tx = cwnd;
        window->num_bits = cwnd;

        log_print(g_log, "state = %d, cwnd = %d, ssthresh = %d rtt_est = %d", state, cwnd, ssthresh, rtt_est);

        while (tx && (current_chunk < seg->num_chunks)) {
            snprintf(comp, MAX_NAME_LENGTH - seg->name->len, "/%d", current_chunk);
            strncpy(str + seg->name->len, comp, seg->name->len);
            i = bit_find(window);
            if (i < 0 || i >= MAX_INTEREST_PIPELINE) {
                /* we must still be waiting for data */
                break;
            }
            chunk_window[i].intr.name = content_name_create(str);
            chunk_window[i].intr.ttl = ttl;
            chunk_window[i].seq_no = current_chunk;
            chunk_window[i].retries = retries;

            _pit_handles[i] = PIT_get_handle(chunk_window[i].intr.name);
            if (!_pit_handles[i]) {
                bit_clear(window, i);
                break;
            }
            pit_to_chunk[_pit_handles[i]->index] = i;

            pthread_mutex_unlock(_pit_handles[i]->mutex);
            ccnfdnb_fwd_interest(&chunk_window[i].intr);
            tx--;
            current_chunk++;
            log_print(g_log, "expressing new interest: %s", chunk_window[i].intr.name->full_name);
        }

        pthread_mutex_lock(&seg_q.mutex);
            struct timespec wait;
            ts_fromnow(&wait);
            ts_addms(&wait, 2 * rtt_est);

            rv = pthread_cond_timedwait(&seg_q.cond, &seg_q.mutex, &wait);
            if ((rv == ETIMEDOUT) && !seg_q.rcv_chunks->len) {
                /* we timed out, we need to rtx */
                rtt_est += rtt_est / 2;
                if (rtt_est > PIT_LIFETIME_MS)
                    rtt_est = PIT_LIFETIME_MS / 2;
            }

            while (seg_q.rcv_chunks->len > 0) {
                PENTRY pe = linked_list_remove(seg_q.rcv_chunks, 0);
                if (!pe) continue;

                pthread_mutex_lock(pe->mutex);

                int chunk_id = pit_to_chunk[pe->index];
                if (chunk_id < 0) {
                    pthread_mutex_unlock(pe->mutex);
                    continue;
                }

                if (chunk_window[chunk_id].seq_no == 0) {
                    seg->obj->publisher = (*pe->obj)->publisher;
                    seg->obj->timestamp = (*pe->obj)->timestamp;
                    seg->chunk_size = (*pe->obj)->size;
                }
                int offset = chunk_window[chunk_id].seq_no * seg->chunk_size;
                memcpy(&seg->obj->data[offset], (*pe->obj)->data, (*pe->obj)->size);

                struct timespec now;
                ts_fromnow(&now);
                ts_addms(&now, PIT_LIFETIME_MS);
                rtt_est = (int)((rtt_est + ts_mselapsed(&now, pe->expires)) / 2.0);
                if (rtt_est < min_rtt_est) {
                    rtt_est = min_rtt_est;
                }

                pit_to_chunk[pe->index] = -1;
                PIT_release(pe);
                free(_pit_handles[chunk_id]);
                _pit_handles[chunk_id] = NULL;
                bit_clear(window, chunk_id);
                bit_set(missing, chunk_window[chunk_id].seq_no);
                log_print(g_log, "fulfilled interest %s", chunk_window[chunk_id].intr.name->full_name);
                content_name_delete(chunk_window[chunk_id].intr.name);
                chunk_window[chunk_id].intr.name = NULL;
                cwnd++;
                if (state == CONG_AVOID)
                    fullfilled++;
            }
        pthread_mutex_unlock(&seg_q.mutex);

        for (i = 0; i < MAX_INTEREST_PIPELINE; i++) {
            if (bit_test(window, i)) {
                if (!_pit_handles[i]) {
                    continue;
                }
                pthread_mutex_lock(_pit_handles[i]->mutex);
                if (PIT_age(_pit_handles[i]) > (2 * rtt_est)) {
                    PIT_refresh(_pit_handles[i]);
                    chunk_window[i].retries--;
                    ccnfdnb_fwd_interest(&chunk_window[i].intr);
                    log_print(g_log, "rtx interest: %s", chunk_window[i].intr.name->full_name);
                    ssthresh = cwnd / 2 + 1;
                    cwnd = 1;
                    state = SLOW_START;
                }
                pthread_mutex_unlock(_pit_handles[i]->mutex);
            }
        }

        if ((cwnd >= ssthresh) && (state == SLOW_START))
            state = CONG_AVOID;
        if (state == SLOW_START)
            fullfilled = 0;
        if ((fullfilled == cwnd) && (state == CONG_AVOID)) {
            cwnd++;
            fullfilled = 0;
        }
        if (cwnd > MAX_INTEREST_PIPELINE)
            cwnd = MAX_INTEREST_PIPELINE;

        /*log_print(g_log, "cwnd = %d, ssthresh = %d", cwnd, ssthresh);*/
    }

    log_print(g_log, "retrieve_segment: finished for %s[%d-%d]",
              seg->name->full_name, 0, seg->num_chunks-1);

    rv = 0;

    PIT_print();
    ccnfdnl_unreg_segment(&seg_q);
    pthread_mutex_destroy(&seg_q.mutex);
    pthread_cond_destroy(&seg_q.cond);
    while (seg_q.rcv_chunks->len) {
        PENTRY pe = linked_list_remove(seg_q.rcv_chunks, 0);
        content_obj_destroy(*pe->obj);
        PIT_release(pe);
    }

    bit_destroy(window);
    bit_destroy(missing);

    return rv;
}

static void * seq_response(void * arg)
{
    prctl(PR_SET_NAME, "ccnfd_seq", 0, 0, 0);
    int sock;
    memcpy(&sock, (int * )arg, sizeof(int));
    free(arg);

    uint32_t payload_size;
    uint32_t name_len;
    char str[MAX_NAME_LENGTH];
    struct content_name * name = NULL;
    int chunks;
    int file_len;
    int rv = -1;
    struct segment seg;

    if (recv(sock, &payload_size, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        send(sock, &rv, sizeof(uint32_t), 0);
        goto END_SEQ_RESP;
    }

    if (recv(sock, &name_len, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        send(sock, &rv, sizeof(uint32_t), 0);
        goto END_SEQ_RESP;
    }

    if (name_len > MAX_NAME_LENGTH)
        name_len = MAX_NAME_LENGTH - 1;

    if (recv(sock, str, name_len, 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        send(sock, &rv, sizeof(uint32_t), 0);
        goto END_SEQ_RESP;
    }
    str[name_len] = '\0';

    if (recv(sock, &chunks, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        send(sock, &rv, sizeof(uint32_t), 0);
        goto END_SEQ_RESP;
    }

    if (recv(sock, &file_len, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "recv: %s.", strerror(errno));
        send(sock, &rv, sizeof(uint32_t), 0);
        goto END_SEQ_RESP;
    }

    name = content_name_create(str);

    if ((seg.obj = CS_getSegment(name)) != NULL) {
        log_print(g_log, "seq_response: found segment %s in CS", name->full_name);
        rv = 0;
        goto RETURN_CONTENT;
    }

    log_print(g_log, "seq_response: retrieving %d chunks with base: %s",
              chunks, name->full_name);

    /* tell the broadcaster not to query the strategy layer to lower overhead */
    ccnfdnb_opt_t opts;
    opts.mode = 0;

    /* we add a dummy component of maximum length to make sure all the segment
     * names willl be valid.
     */
    snprintf(str, MAX_NAME_LENGTH - name->len - 1, "%d", chunks);
    if (content_name_appendComponent(name, str) != 0) {
        log_print(g_log, "could not append component to name (too long?)");
        send(sock, &rv, sizeof(uint32_t), 0);
        goto END_SEQ_RESP;
    }
    int chunk_size = (chunks > 1) ? ccnf_max_payload_size(name) : file_len;
    content_name_removeComponent(name, name->num_components - 1);

    completed_jobs_t dld_segments;
	pthread_mutex_init(&dld_segments.mutex, NULL);
	pthread_cond_init(&dld_segments.cond, NULL);
	dld_segments.completed = linked_list_init(free);

    seg.name = name;
    seg.num_chunks = chunks;
    seg.opts = &opts;
    seg.chunk_size = chunk_size - 1;
    seg.obj = malloc(sizeof(struct content_obj));
    seg.obj->name = name;
    seg.obj->data = malloc(file_len);
    if (!seg.obj->data) {
        log_print(g_log, "retrieve_segment: failed to allocated %d bytes!", file_len);
        send(sock, &rv, sizeof(uint32_t), 0);
        goto END_SEQ_RESP;
    }
    seg.obj->size = file_len;

    if ((rv = retrieve_segment(&seg)) < 0) {
        log_print(g_log, "retrieve_segment: failed to get %s!", name->full_name);
        send(sock, &rv, sizeof(uint32_t), 0);
        goto END_SEQ_RESP;
    }

    /* return the content */
    RETURN_CONTENT:
    if (send(sock, &rv, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "send: %s.", strerror(errno));
        goto END_SEQ_RESP;
    }

    if (send(sock, &seg.obj->publisher, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "send: %s.", strerror(errno));
        goto END_SEQ_RESP;
    }

    if (send(sock, &name->len, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "send: %s.", strerror(errno));
        goto END_SEQ_RESP;
    }

    if (send(sock, name->full_name, name_len, 0) == -1) {
        log_print(g_log, "send: %s.", strerror(errno));
        goto END_SEQ_RESP;
    }

    if (send(sock, &seg.obj->timestamp, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "send: %s.", strerror(errno));
        goto END_SEQ_RESP;
    }

    if (send(sock, &seg.obj->size, sizeof(uint32_t), 0) == -1) {
        log_print(g_log, "send: %s.", strerror(errno));
        goto END_SEQ_RESP;
    }

    int total = 0;
    int left = seg.obj->size;
    int n = -1;

    while (total < seg.obj->size) {
        log_print(g_log, "total = %d", total);
        n = send(sock, seg.obj->data+total, left, 0);
        if (n == -1) break;
        total += n;
        left -= n;
        log_print(g_log, "seq_response: sent %d bytes, %d bytes to go", n, left);
    }

    if (n == -1 || left != 0 || total != seg.obj->size) {
        log_print(g_log, "encountered error, sending segment, sent %d bytes!", total);
    } else {
        log_print(g_log, "seq_response: returned %d bytes", total);
    }

    content_obj_destroy(seg.obj);

    END_SEQ_RESP:

    close(sock);

    return NULL;
}
