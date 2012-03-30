#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
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

#include "ccnf.h"
#include "ccnfd.h"
#include "ccnfd_constants.h"
#include "ccnf_packet.h"
#include "net_lib.h"

static int verify_content(struct content_obj * content)
{
    if (!content) {
        fprintf(stderr, "content invalid\n");
        return -1;
    }

    if (!content->name || !content->data) {
        fprintf(stderr, "content name or data invalid\n");
        return -1;
    }

    if (content->size <= 0) {
        fprintf(stderr, "content size = %d\n", content->size);
        return -1;
    }

    if (content->size > ccnf_max_payload_size(content->name)) {
        fprintf(stderr, "content size too large!!\n");
        return -1;
    }

    if (!content->name->full_name) {
        fprintf(stderr, "content name invalid\n");
        return -1;
    }

    return 0;
}

inline void ccnf_did2sockpath(uint32_t daemonId, char * str, int size)
{
    snprintf(str, 256, "/tmp/ccnfd_%u.sock", daemonId);
}

inline int ccnf_max_payload_size(struct content_name * name)
{
    return CCNF_MAX_PACKET_SIZE - MIN_DATA_PKT_SIZE - name->len;
}

int ccnf_set_timeout(unsigned timeout_ms)
{
    int s;
    struct sockaddr_un daemon;

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        close (s);
        return -1;
    }

    daemon.sun_family = AF_UNIX;
    char sock_path[256];
    ccnf_did2sockpath(IP4_to_nodeId(), sock_path, 256);
    strcpy(daemon.sun_path, sock_path);
    int len = strlen(daemon.sun_path) + sizeof(daemon.sun_family);
    if (connect(s, (struct sockaddr * ) &daemon, len) == -1) {
        perror("connect");
        close (s);
        return -1;
    }

    char buf[sizeof(uint8_t)+ sizeof(uint32_t) + sizeof(uint32_t)];

    uint8_t type = MSG_IPC_TIMEOUT;
    uint32_t msg_size = sizeof(uint32_t);
    uint32_t payload = (uint32_t)timeout_ms;
    memcpy(buf, &type, sizeof(uint8_t));
    memcpy(buf+sizeof(uint8_t), &msg_size, sizeof(uint32_t));
    memcpy(buf+sizeof(uint8_t)+sizeof(uint32_t), &payload, sizeof(uint32_t));

    /* send the req */
    if (send(s, buf, sizeof(type) + sizeof(msg_size) + sizeof(payload), 0) == -1) {
        perror("send");
        close (s);
        return -1;
    }

    close(s);

    return 0;
}

int ccnf_set_retries(unsigned max_attempts)
{
    int s;
    struct sockaddr_un daemon;

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        close (s);
        return -1;
    }

    daemon.sun_family = AF_UNIX;
    char sock_path[256];
    ccnf_did2sockpath(IP4_to_nodeId(), sock_path, 256);
    strcpy(daemon.sun_path, sock_path);
    int len = strlen(daemon.sun_path) + sizeof(daemon.sun_family);
    if (connect(s, (struct sockaddr * ) &daemon, len) == -1) {
        perror("connect");
        close (s);
        return -1;
    }

    char buf[sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t)];

    uint8_t type = MSG_IPC_RETRIES;
    uint32_t msg_size = sizeof(uint32_t);
    uint32_t payload = (uint32_t)max_attempts;
    memcpy(buf, &type, sizeof(uint8_t));
    memcpy(buf+sizeof(uint8_t), &msg_size, sizeof(uint32_t));
    memcpy(buf+sizeof(uint8_t)+sizeof(uint32_t), &payload, sizeof(uint32_t));

    /* send the req */
    if (send(s, buf, sizeof(type) + sizeof(msg_size) + sizeof(payload), 0) == -1) {
        perror("send");
        close (s);
        return -1;
    }

    close(s);

    return 0;
}

int ccnf_publishSeq(struct content_obj * index_obj, struct linked_list * chunks)
{
    if (verify_content(index_obj) != 0) {
        fprintf(stderr, "ccnf_publish: found a problem with content -- IGNORING");
        return -1;
    }

    int i;
    for (i = 0; i < chunks->len; i++) {
        if (verify_content(linked_list_get(chunks, i)) != 0)
            return -1;
    }

    int s;
    struct sockaddr_un daemon;

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        close (s);
        return -1;
    }

    daemon.sun_family = AF_UNIX;
    char sock_path[256];
    ccnf_did2sockpath(IP4_to_nodeId(), sock_path, 256);
    strcpy(daemon.sun_path, sock_path);
    int len = strlen(daemon.sun_path) + sizeof(daemon.sun_family);
    if (connect(s, (struct sockaddr * ) &daemon, len) == -1) {
        perror("connect");
        close (s);
        return -1;
    }

    char buf[1024];

    /* structure  of publish msg:
     * publisher : int
     * name_len : int
     * name : char[name_len]
     * timestamp : int
     * size : int
     * data : byte[size]
     */
    uint32_t publisher = IP4_to_nodeId();
    uint32_t name_len = strnlen(index_obj->name->full_name, index_obj->name->len);
    uint8_t * name = (uint8_t *)index_obj->name->full_name;
    uint32_t timestamp = index_obj->timestamp;
    uint32_t size = index_obj->size;
    uint8_t * data = index_obj->data;

    uint8_t type = MSG_IPC_SEQ_PUBLISH;
    uint32_t payload_size = 4*sizeof(uint32_t) + name_len + size;

    int offset = 0;
    memcpy(buf, &type, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    memcpy(buf+offset, &payload_size, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(buf+offset, &publisher, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(buf+offset, &name_len, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    if (send(s, buf, offset, 0) == -1) {
        perror("send");
        close (s);
        return -1;
    }

    if (send(s, name, name_len, 0) == -1) {
        perror("send");
        close (s);
        return -1;
    }

    offset = 0;
    memcpy(buf, &timestamp, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(buf+offset, &size, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    if (send(s, buf, offset, 0) == -1) {
        perror("send");
        close (s);
        return -1;
    }

    if (send(s, data, size, 0) == -1) {
        perror("send");
        close (s);
        return -1;
    }

    if (send(s, &chunks->len, sizeof(uint32_t), 0) == -1) {
        perror("send");
        close (s);
        return -1;
    }

    for (i = 0; i < chunks->len; i++) {
        struct content_obj * obj = linked_list_get(chunks, i);
        publisher = IP4_to_nodeId();
        name_len = strnlen(obj->name->full_name, obj->name->len);
        name = (uint8_t *)obj->name->full_name;
        timestamp = obj->timestamp;
        size = obj->size;
        data = obj->data;
        payload_size = 4*sizeof(uint32_t) + name_len + size;

        int offset = 0;
        memcpy(buf+offset, &payload_size, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        memcpy(buf+offset, &publisher, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        memcpy(buf+offset, &name_len, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        if (send(s, buf, offset, 0) == -1) {
            perror("send");
            close (s);
            return -1;
        }

        if (send(s, name, name_len, 0) == -1) {
            perror("send");
            close (s);
            return -1;
        }

        offset = 0;
        memcpy(buf, &timestamp, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        memcpy(buf+offset, &size, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        if (send(s, buf, offset, 0) == -1) {
            perror("send");
            close (s);
            return -1;
        }

        if (send(s, data, size, 0) == -1) {
            perror("send");
            close (s);
            return -1;
        }
    }

    /* get daemon rv */
    int rv = -1;
    if (recv(s, &rv, sizeof(uint32_t), 0) == -1) {
        perror("recv");
    }

    close (s);
    return rv;
}

int ccnf_publish(struct content_obj * content)
{
    if (verify_content(content) != 0) {
        fprintf(stderr, "ccnf_publish: found a problem with content -- IGNORING");
        return -1;
    }

    int s;
    struct sockaddr_un daemon;

    content->publisher = IP4_to_nodeId();

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        close (s);
        return -1;
    }

    daemon.sun_family = AF_UNIX;
    char sock_path[256];
    ccnf_did2sockpath(IP4_to_nodeId(), sock_path, 256);
    strcpy(daemon.sun_path, sock_path);
    int len = strlen(daemon.sun_path) + sizeof(daemon.sun_family);
    if (connect(s, (struct sockaddr * ) &daemon, len) == -1) {
        perror("connect");
        close (s);
        return -1;
    }

    char buf[1024];

    /* structure  of publish msg:
     * publisher : int
     * name_len : int
     * name : char[name_len]
     * timestamp : int
     * size : int
     * data : byte[size]
     */
    uint32_t publisher = content->publisher;
    uint32_t name_len = strnlen(content->name->full_name, content->name->len);
    uint8_t * name = (uint8_t *)content->name->full_name;
    uint32_t timestamp = content->timestamp;
    uint32_t size = content->size;
    uint8_t * data = content->data;

    uint8_t type = MSG_IPC_PUBLISH;
    uint32_t payload_size = 4*sizeof(uint32_t) + name_len + size;

    int offset = 0;
    memcpy(buf, &type, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    memcpy(buf+offset, &payload_size, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(buf+offset, &publisher, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(buf+offset, &name_len, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    if (send(s, buf, offset, 0) == -1) {
        perror("send");
        close (s);
        return -1;
    }

    if (send(s, name, name_len, 0) == -1) {
        perror("send");
        close (s);
        return -1;
    }

    offset = 0;
    memcpy(buf, &timestamp, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(buf+offset, &size, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    if (send(s, buf, offset, 0) == -1) {
        perror("send");
        close (s);
        return -1;
    }

    if (send(s, data, size, 0) == -1) {
        perror("send");
        close (s);
        return -1;
    }

    /* get daemon rv */
    int rv = -1;
    if (recv(s, &rv, sizeof(uint32_t), 0) == -1) {
        perror("recv");
    }

    close (s);
    return rv;
}

int ccnf_retrieve(struct content_name * name, struct content_obj ** content_ptr)
{
    if (!content_ptr || !name) {
        fprintf(stderr, "ccnf_retrive: content pointer/name invalid! -- IGNORING");
        return -1;
    }

    int s;
    struct sockaddr_un daemon;

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        close (s);
        return -1;
    }

    daemon.sun_family = AF_UNIX;
    char sock_path[256];
    ccnf_did2sockpath(IP4_to_nodeId(), sock_path, 256);
    strcpy(daemon.sun_path, sock_path);
    int len = strlen(daemon.sun_path) + sizeof(daemon.sun_family);
    if (connect(s, (struct sockaddr * ) &daemon, len) == -1) {
        perror("connect");
        close (s);
        return -1;
    }

    char buf[MAX_NAME_LENGTH + sizeof(uint32_t) + sizeof(uint8_t)];

    uint8_t type = MSG_IPC_RETRIEVE;

    uint32_t msg_size = name->len + sizeof(uint32_t);
    memcpy(buf, &type, sizeof(uint8_t));
    memcpy(buf+sizeof(uint8_t), &msg_size, sizeof(uint32_t));
    memcpy(buf+sizeof(uint8_t)+sizeof(uint32_t), &name->len, sizeof(uint32_t));
    memcpy(buf+sizeof(uint8_t)+sizeof(uint32_t)+sizeof(uint32_t), name->full_name, name->len);

    /* send the req */
    if (send(s, buf, sizeof(uint8_t) + sizeof(uint32_t) + msg_size, 0) == -1) {
        perror("send");
        close (s);
        return -1;
    }

    /* structure  of retrieve response msg:
     * rv : int
     * publisher : int
     * name_len : int
     * name : char[name_len]
     * timestamp : int
     * size : int
     * data : byte[size]
     */

    /* get the response */
    int rv;
    if (recv(s, &rv, sizeof(int), 0) < sizeof(uint32_t)) {
        perror("recv");
        close (s);
        return -1;
    }

    if (rv != 0) {
        /* the content retrieve failed in the daemon! -- don't continue */
        close(s);
        return rv;
    }

    *content_ptr = (struct content_obj *) malloc(sizeof(struct content_obj));
    uint32_t publisher;
    struct content_obj * content = *content_ptr;
    content->name = NULL;
    uint32_t name_len;
    char * str = NULL;
    uint32_t timestamp;
    uint32_t size;
    uint8_t * data = NULL;

    if (recv(s, &publisher, sizeof(uint32_t), 0) < sizeof(uint32_t)) {
        perror("recv");
        rv = -1;
        goto END_RETRIEVE;
    }

    if (recv(s, &name_len, sizeof(uint32_t), 0) < sizeof(uint32_t)) {
        perror("recv");
        rv = -1;
        goto END_RETRIEVE;
    }

    str = (char * ) malloc(name_len + 1);
    if (recv(s, str, name_len, 0) < name_len) {
        perror("recv");
        rv = -1;
        goto END_RETRIEVE;
    }
    str[name_len] = '\0';

    content->name = content_name_create(str);
    free(str);

    if (recv(s, &timestamp, sizeof(uint32_t), 0) < sizeof(uint32_t)) {
        perror("recv");
        rv = -1;
        goto END_RETRIEVE;
    }
    content->timestamp = timestamp;

    if (recv(s, &size, sizeof(uint32_t), 0) < sizeof(uint32_t)) {
        perror("recv");
        rv = -1;
        goto END_RETRIEVE;
    }
    content->size = size;

    data = (uint8_t * ) malloc(size);
    if (recv(s, data, size, 0) < size) {
        perror("recv");
        rv = -1;
        goto END_RETRIEVE;
    }
    content->data = data;

    END_RETRIEVE:
    if (rv != 0) {
        /* error */
        if (data) free(data);
        if (content->name) content_name_delete(content->name);
        free(content);
    }

    close(s);

    return rv;
}

int ccnf_retrieveSeq(struct content_name * baseName, int chunks, int file_len, struct content_obj ** content_ptr)
{
    if (!content_ptr || !baseName) {
        fprintf(stderr, "ccnf_retrive: content pointer/name invalid! -- IGNORING");
        return -1;
    }

    int s;
    struct sockaddr_un daemon;

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        close (s);
        return -1;
    }

    daemon.sun_family = AF_UNIX;
    char sock_path[256];
    ccnf_did2sockpath(IP4_to_nodeId(), sock_path, 256);
    strcpy(daemon.sun_path, sock_path);
    int len = strlen(daemon.sun_path) + sizeof(daemon.sun_family);
    if (connect(s, (struct sockaddr * ) &daemon, len) == -1) {
        perror("connect");
        close (s);
        return -1;
    }

    uint8_t type = MSG_IPC_SEQ_RETRIEVE;
    uint32_t msg_size = baseName->len + 4 * sizeof(uint32_t);
    /* send the req */
    if (send(s, &type, sizeof(uint8_t), 0) == -1) {
        perror("send");
        close (s);
        return -1;
    }
    if (send(s, &msg_size, sizeof(uint32_t), 0) == -1) {
        perror("send");
        close (s);
        return -1;
    }
    if (send(s, &baseName->len, sizeof(uint32_t), 0) == -1) {
        perror("send");
        close (s);
        return -1;
    }
    if (send(s, baseName->full_name, baseName->len, 0) == -1) {
        perror("send");
        close (s);
        return -1;
    }
    if (send(s, &chunks, sizeof(uint32_t), 0) == -1) {
        perror("send");
        close (s);
        return -1;
    }
    if (send(s, &file_len, sizeof(uint32_t), 0) == -1) {
        perror("send");
        close (s);
        return -1;
    }

    /* structure  of sequence response msg:
     * rv : int
     * publisher : int
     * name_len : int
     * name : char[name_len]
     * timestamp : int
     * size : int
     * data : byte[size]
     */

    /* get the response */
    int rv;
    if (recv(s, &rv, sizeof(int), 0) < sizeof(uint32_t)) {
        perror("recv");
        close (s);
        return -1;
    }

    if (rv != 0) {
        /* the content retrieve failed in the daemon! -- don't continue */
        close(s);
        return rv;
    }

    *content_ptr = (struct content_obj *) malloc(sizeof(struct content_obj));
    uint32_t publisher;
    struct content_obj * content = *content_ptr;
    content->name = NULL;
    uint32_t name_len;
    char * str = NULL;
    uint32_t timestamp;
    uint32_t size;
    uint8_t * data = NULL;

    if (recv(s, &publisher, sizeof(uint32_t), 0) < sizeof(uint32_t)) {
        perror("recv");
        rv = -1;
        goto END_RETRIEVE;
    }

    if (recv(s, &name_len, sizeof(uint32_t), 0) < sizeof(uint32_t)) {
        perror("recv");
        rv = -1;
        goto END_RETRIEVE;
    }

    str = (char * ) malloc(name_len + 1);
    if (recv(s, str, name_len, 0) < name_len) {
        perror("recv");
        rv = -1;
        goto END_RETRIEVE;
    }
    str[name_len] = '\0';

    content->name = content_name_create(str);
    free(str);

    if (recv(s, &timestamp, sizeof(uint32_t), 0) < sizeof(uint32_t)) {
        perror("recv");
        rv = -1;
        goto END_RETRIEVE;
    }
    content->timestamp = timestamp;

    if (recv(s, &size, sizeof(uint32_t), 0) < sizeof(uint32_t)) {
        perror("recv");
        rv = -1;
        goto END_RETRIEVE;
    }
    content->size = size;

    data = (uint8_t * ) malloc(size);
    int total = 0;
    int left = size;
    int n = -1;

    while (total < size) {
        n = recv(s, data+total, left, 0);
        if (n == -1) break;
        total += n;
        left -= n;
    }

    content->data = data;

    END_RETRIEVE:
    if (rv != 0) {
        /* error */
        if (data) free(data);
        if (content->name) content_name_delete(content->name);
        free(content);
    }

    close(s);

    return rv;
}

int ccnf_cs_summary(struct bloom ** bloom_ptr)
{
    if (!bloom_ptr) {
        syslog(LOG_WARNING, "ccnf_cs_summary: invalid bloom_ptr! -- IGNORING");
        return -1;
    }

    int s;
    struct sockaddr_un daemon;

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        close (s);
        return -1;
    }

    daemon.sun_family = AF_UNIX;
    char sock_path[256];
    ccnf_did2sockpath(IP4_to_nodeId(), sock_path, 256);
    strcpy(daemon.sun_path, sock_path);
    int len = strlen(daemon.sun_path) + sizeof(daemon.sun_family);
    if (connect(s, (struct sockaddr * ) &daemon, len) == -1) {
        perror("connect");
        close (s);
        return -1;
    }

    char buf[256];

    uint8_t type = MSG_IPC_CS_SUMMARY_REQ;
    uint32_t msg_size = 1;
    uint32_t payload = 0;
    memcpy(buf, &type, sizeof(uint8_t));
    memcpy(buf+sizeof(uint8_t), &msg_size, sizeof(uint32_t));
    memcpy(buf+sizeof(uint8_t)+sizeof(uint32_t), &payload, sizeof(uint32_t));

    /* send the req */
    if (send(s, buf, sizeof(type) + sizeof(msg_size) + sizeof(payload), 0) == -1) {
        perror("send");
        close (s);
        return -1;
    }
    /* retrieve the response */
    uint32_t size = 1;
    int n;
    if ((n = recv(s, &size, sizeof(uint32_t), 0)) < sizeof(uint32_t)) {
        perror("recv");
        close (s);
        return -1;
    }

    uint32_t * vector = (uint32_t * ) calloc(size, sizeof(uint32_t));
    if ((n = recv(s, vector, size * sizeof(uint32_t), 0)) < size * sizeof(uint32_t)) {
        perror("recv");
        close (s);
        return -1;
    }

    int bits = size * BITS_PER_WORD;
    *bloom_ptr = bloom_createFromVector(bits, vector, BLOOM_ARGS);
    free(vector);
    close (s);

    return 0;
}
