#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#include "net_buffer.h"
#include "net_lib.h"

int net_buffer_init(int size, struct net_buffer * buf)
{
	if (!buf) return -1;

    buf->buf = (uint8_t * ) malloc(sizeof(uint8_t) * size);
    buf->size = size;
    buf->buf_ptr = buf->buf;
    return 0;
}

/* size in bytes */
struct net_buffer * net_buffer_create(int size)
{
    struct net_buffer * buf;
    buf = (struct net_buffer * ) malloc(sizeof(struct net_buffer));
    buf->buf = (uint8_t * ) malloc(sizeof(uint8_t) * size);
    buf->size = size;
    buf->buf_ptr = buf->buf;
    return buf;
}

struct net_buffer * net_buffer_createFrom(uint8_t * b, int size)
{
    struct net_buffer * buf;
    buf = (struct net_buffer * ) malloc(sizeof(struct net_buffer));
    buf->buf = (uint8_t * ) malloc(sizeof(uint8_t) * size);
    buf->size = size;
    buf->buf_ptr = buf->buf;
    memcpy(buf->buf, b, size);
    return buf;
}

void net_buffer_delete(struct net_buffer * buf)
{
    free(buf->buf);
    buf->buf = buf->buf_ptr = NULL;
    free(buf);
}

int net_buffer_putLong(struct net_buffer * buf, uint64_t lword)
{
    if (!buf) return -1;
    if ((buf->buf_ptr + sizeof(uint64_t)) > buf->buf+buf->size) return -1;

    putLong(buf->buf_ptr, lword);
    buf->buf_ptr += sizeof(uint64_t);
    return 0;
}

int net_buffer_putInt(struct net_buffer * buf, uint32_t word)
{
    if (!buf) return -1;
    if ((buf->buf_ptr + sizeof(uint32_t)) > buf->buf+buf->size) return -1;

    putInt(buf->buf_ptr, word);
    buf->buf_ptr += sizeof(uint32_t);
    return 0;
}

int net_buffer_putShort(struct net_buffer * buf, uint16_t half)
{
    if (!buf) return -1;
    if ((buf->buf_ptr + sizeof(uint16_t)) > buf->buf+buf->size) return -1;

    putShort(buf->buf_ptr, half);
    buf->buf_ptr += sizeof(uint16_t);
    return 0;
}

int net_buffer_putByte(struct net_buffer * buf, uint8_t byte)
{
    if (!buf) return -1;
    if ((buf->buf_ptr + sizeof(uint8_t)) > buf->buf+buf->size) return -1;

    putByte(buf->buf_ptr, byte);
    buf->buf_ptr += sizeof(uint8_t);
    return 0;
}

uint64_t net_buffer_getLong(struct net_buffer * buf)
{
    if (!buf) return -1;
    if ((buf->buf_ptr + sizeof(uint64_t)) > buf->buf+buf->size) return -1;

    uint64_t target = getLong(buf->buf_ptr);
    buf->buf_ptr += sizeof(uint64_t);
    return target;
}

uint32_t net_buffer_getInt(struct net_buffer * buf)
{
    if (!buf) return -1;
    if ((buf->buf_ptr + sizeof(uint32_t)) > buf->buf+buf->size) return -1;

    uint32_t target = getInt(buf->buf_ptr);
    buf->buf_ptr += sizeof(uint32_t);
    return target;
}

uint16_t net_buffer_getShort(struct net_buffer * buf)
{
    if (!buf) return -1;
    if ((buf->buf_ptr + sizeof(uint16_t)) > buf->buf+buf->size) return -1;

    uint16_t target = getShort(buf->buf_ptr);
    buf->buf_ptr += sizeof(uint16_t);
    return target;
}

uint8_t net_buffer_getByte(struct net_buffer * buf)
{
    if (!buf) return -1;
    if ((buf->buf_ptr + sizeof(uint8_t)) > buf->buf+buf->size) return -1;

    uint8_t target = getByte(buf->buf_ptr);
    buf->buf_ptr += sizeof(uint8_t);
    return target;
}

int net_buffer_copyFrom(struct net_buffer * src, void * dst, int size)
{
    if (!src || !dst) return -1;
    if (!src->buf) return -1;

    memcpy(dst, src->buf_ptr, size);
    src->buf_ptr += sizeof(uint8_t) * size;
    return 0;
}

int net_buffer_copyTo(struct net_buffer * dst, void * src, int size)
{
    if (!src || !dst) return -1;
    if (!dst->buf) return -1;
	if ((dst->buf_ptr + size) > (dst->buf + dst->size)) return -1;

    memcpy(dst->buf_ptr, src, size);
    dst->buf_ptr += sizeof(uint8_t) * size;
    return 0;
}

int net_buffer_send(struct net_buffer * buf, int sock, struct sockaddr_in * addr)
{
    if (sock < 0 || !buf || !addr) return -1;

    if (sendto(sock, buf->buf, buf->buf_ptr - buf->buf, 0,
               (struct sockaddr * ) addr, sizeof(struct sockaddr_in)) == -1) {
        syslog(LOG_ERR, "net_buffer_send: sendto failed - %s.", strerror(errno));
        return -1;
    }

    return 0;
}

int net_buffer_recv(struct net_buffer * buf, int sock, struct sockaddr_in * addr)
{
    if (sock < 0 || !buf || !addr) return -1;

    int rv = -1;
    socklen_t len = sizeof(struct sockaddr_in);
    if ((rv = recvfrom(sock, buf->buf, buf->size, 0,
                 (struct sockaddr * ) addr, &len)) == -1) {
        syslog(LOG_ERR, "net_buffer_recv: recvfrom failed - %s.", strerror(errno));
        return -1;
    }

    return rv;
}

int net_buffer_reset(struct net_buffer * buf)
{
	buf->buf_ptr = buf->buf;
	return 0;
}

