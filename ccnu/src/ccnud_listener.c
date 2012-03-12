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
#include <math.h>
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
#include "ccnud_pit.h"

#include "ccnud_constants.h"
#include "ccnud_listener.h"
#include "ccnud_net_broadcaster.h"

#include "bitmap.h"
#include "log.h"
#include "net_buffer.h"
#include "synch_queue.h"
#include "thread_pool.h"
#include "ts.h"

struct ccnud_listener {
    int sock;
    struct sockaddr_un local;
    thread_pool_t interest_pipeline;
    int interest_pipe_size;
};

struct segment {
    struct content_name * name;
    ccnudnb_opt_t * opts;
    int num_chunks;
    struct content_obj * obj;
    int chunk_size;
};

struct chunk {
    int seq_no;
    struct ccnu_interest_pkt intr;
    int retries;
};

struct ccnud_listener _listener;

static void * create_summary_response(void * arg);
static void * publish_response(void * arg);
static void * retrieve_response(void * arg);
static void * seq_response(void * arg);

int ccnudl_init(int pipeline_size)
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

    _listener.interest_pipe_size = pipeline_size;
    if (tpool_create(&_listener.interest_pipeline, INTEREST_FLOWS) < 0) {
        log_print(g_log, "tpool_create: could not create interest thread pool!");
        return -1;
    }

    log_print(g_log, "interest window: %d", _listener.interest_pipe_size);
    log_print(g_log, "max interest window: %d", MAX_INTEREST_PIPELINE);

    return 0;
}

int ccnudl_close()
{
    close(_listener.sock);
    tpool_shutdown(&_listener.interest_pipeline);
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
    log_print(g_log, "ccnud_accept: recieved a message of type: %d and size: %d", msg->type, n);

    return 0;
}

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

    return NULL;
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
    return NULL;
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
//        rv = ccnudnb_express_interest(seg->name, &content, 1, seg->opts);
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
//            if (ccnumr_sendWhere(seg->name,
//                                 &seg->opts->orig_level_u, &seg->opts->orig_clusterId_u,
//                                 &seg->opts->dest_level_u, &seg->opts->dest_clusterId_u,
//                                 &seg->opts->distance) < 0) {
//                log_print(g_log, "seq_response: sendWhere? failed! -- cannot send interest, %s!",
//                          seg->name->full_name);
//                rv = -1;
//                goto END;
//            }
//            rv = ccnudnb_express_interest(seg->name, &content, 1, seg->opts);
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

    double distance = seg->opts->distance;
    unsigned orig_level_u = seg->opts->orig_level_u;
    unsigned orig_clusterId_u = seg->opts->orig_clusterId_u;
    unsigned dest_level_u = seg->opts->dest_level_u;
    unsigned dest_clusterId_u = seg->opts->dest_clusterId_u;
    int retries = INTEREST_MAX_ATTEMPTS;
    int timeout_ms = INTEREST_TIMEOUT_MS;
    int ttl = MAX_TTL;

    if ((seg->opts->mode & CCNUDNB_USE_RETRIES) == CCNUDNB_USE_RETRIES) {
        retries = seg->opts->retries;
    }
    if ((seg->opts->mode & CCNUDNB_USE_TIMEOUT) == CCNUDNB_USE_TIMEOUT) {
        timeout_ms = seg->opts->timeout_ms;
    }
    if ((seg->opts->mode & CCNUDNB_USE_TTL) == CCNUDNB_USE_TTL) {
        ttl = seg->opts->ttl;
    }

    int min_window_size = 1;
    int max_window_size = MAX_INTEREST_PIPELINE;
    int window_size = min_window_size;
    struct bitmap * missing = bit_create(seg->num_chunks);
    struct bitmap * window = bit_create(max_window_size);
    window->num_bits = window_size;
    struct chunk * chunk_window = malloc(sizeof(struct chunk) * max_window_size);
    PENTRY * _pit_handles = (PENTRY * ) calloc(sizeof(PENTRY), max_window_size);

    char str[MAX_NAME_LENGTH], comp[MAX_NAME_LENGTH];
    strncpy(str, seg->name->full_name, seg->name->len);

    int i;
    int current_chunk = 0;
    struct timespec now;
    struct timespec wait;
    struct timespec temp;
    while (!bit_allSet(missing)) {
        /*log_print(g_log, "window_size = %d, %d", window_size, window->num_bits);*/
        while (!bit_allSet(window)) {
            i = bit_find(window);
            if (i < 0) break;
            current_chunk = bit_find(missing);
            if (current_chunk == -1)
                goto RET_ALL;
            snprintf(comp, MAX_NAME_LENGTH - seg->name->len, "/%d", current_chunk);
            strncpy(str + seg->name->len, comp, seg->name->len);
            chunk_window[i].intr.name = content_name_create(str);
            chunk_window[i].intr.ttl = ttl;
            chunk_window[i].intr.orig_level = orig_level_u;
            chunk_window[i].intr.orig_clusterId = orig_clusterId_u;
            chunk_window[i].intr.dest_level = dest_level_u;
            chunk_window[i].intr.dest_clusterId = dest_clusterId_u;
            chunk_window[i].intr.distance = distance;
            chunk_window[i].seq_no = current_chunk;
            chunk_window[i].retries = retries;
            _pit_handles[i] = PIT_get_handle(chunk_window[i].intr.name);

            if (!_pit_handles[i]) {
                log_print(g_log, "retrieve_segment: failed to create pit entry");
                goto END;
            }
            pthread_mutex_unlock(_pit_handles[i]->mutex);
            ccnudnb_fwd_interest(&chunk_window[i].intr);
        }

        pthread_mutex_lock(_pit_handles[window_size-1]->mutex);
            ts_fromnow(&wait);
            ts_addms(&wait, timeout_ms);
            pthread_cond_timedwait(_pit_handles[window_size-1]->cond, _pit_handles[window_size-1]->mutex, &wait);
        pthread_mutex_unlock(_pit_handles[window_size-1]->mutex);

        /* we filled the window */
        ts_fromnow(&now);
        int fullfilled = 0;
        int lost = 0;
        for (i = 0; i < max_window_size; i++) {
            if (bit_test(window, i) == 0) {
                continue;
            }

            pthread_mutex_lock(_pit_handles[i]->mutex);
            if (*_pit_handles[i]->obj) {
                if (chunk_window[i].seq_no == 0) {
                    seg->obj->publisher = (*_pit_handles[i]->obj)->publisher;
                    seg->obj->timestamp = (*_pit_handles[i]->obj)->timestamp;
                }
                int offset = chunk_window[i].seq_no * seg->chunk_size;
                memcpy(seg->obj->data+offset, (*_pit_handles[i]->obj)->data, (*_pit_handles[i]->obj)->size);

                PIT_release(_pit_handles[i]);
                bit_clear(window, i);
                bit_set(missing, chunk_window[i].seq_no);
                content_name_delete(chunk_window[i].intr.name);
                fullfilled++;
            } else {
                temp = _pit_handles[i]->creation;
                ts_addms(&temp, timeout_ms);
                if (ts_compare(&temp, &now) > 0) {
                    pthread_mutex_unlock(_pit_handles[i]->mutex);
                    continue;
                }
                /* we do not have the content yet, rtx interest */
                PIT_refresh(_pit_handles[i]);
                pthread_mutex_unlock(_pit_handles[i]->mutex);
                chunk_window[i].retries--;
                ccnudnb_fwd_interest(&chunk_window[i].intr);
                lost++;
                if (chunk_window[i].retries == 0) {
                    chunk_window[i].retries = retries;
                }


            }
        }

        if (fullfilled && (window_size < max_window_size)) {
            window_size += fullfilled;
            if (window_size > max_window_size)
                window_size = max_window_size;
            window->num_bits = window_size;
            /*log_print(g_log, "increasing window (%d)", window_size);*/
        }
        if (lost > 0) {
            window_size /= 2;
            window_size -= lost;
            if (window_size < min_window_size) {
                window_size = min_window_size;
                /*log_print(g_log, "decreasing window (%d)", window_size);*/
            }
            window->num_bits = window_size;
        }

        log_print(g_log, "window_size = %d", window_size);
    }

    RET_ALL:
    log_print(g_log, "retrieve_segment: finished for %s[%d-%d]",
              seg->name->full_name, 0, seg->num_chunks-1);

    rv = 0;

    END:
    bit_destroy(window);
    bit_destroy(missing);
    free(chunk_window);
    free(_pit_handles);

    return rv;
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
    int chunks;
    int file_len;
    int rv = -1;

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
    log_print(g_log, "seq_response: retrieving %d chunks with base: %s",
              chunks, name->full_name);

    /* tell the broadcaster not to query the strategy layer to lower overhead */
    ccnudnb_opt_t opts;
    opts.mode = CCNUDNB_USE_ROUTE;
    if (ccnumr_sendWhere(name,
                        &opts.orig_level_u, &opts.orig_clusterId_u,
                        &opts.dest_level_u, &opts.dest_clusterId_u,
                        &opts.distance) < 0) {
        log_print(g_log, "seq_response: sendWhere? failed! -- cannot send interest, %s!",
                  name->full_name);
        send(sock, &rv, sizeof(uint32_t), 0);
        goto END_SEQ_RESP;
    }

    /* we add a dummy component of maximum length to make sure all the segment
     * names willl be valid.
     */
    snprintf(str, MAX_NAME_LENGTH - name->len - 1, "%d", chunks);
    if (content_name_appendComponent(name, str) != 0) {
        log_print(g_log, "could not append component to name (too long?)");
        send(sock, &rv, sizeof(uint32_t), 0);
        goto END_SEQ_RESP;
    }
    content_name_removeComponent(name, name->num_components - 1);

    completed_jobs_t dld_segments;
	pthread_mutex_init(&dld_segments.mutex, NULL);
	pthread_cond_init(&dld_segments.cond, NULL);
	dld_segments.completed = linked_list_init(free);

    int chunk_size = ceil((double) file_len / (double)chunks);
    struct segment seg;
    seg.name = name;
    seg.num_chunks = chunks;
    seg.opts = &opts;
    seg.chunk_size = chunk_size;
    seg.obj = malloc(sizeof(struct content_obj));
    seg.obj->name = name;
    seg.obj->data = NULL;
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

    free(seg.obj->data);
    content_name_delete(seg.obj->name);
    free(seg.obj);

    END_SEQ_RESP:

    close(sock);

    return NULL;
}
