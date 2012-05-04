#ifndef NET_BUFFER_H_INCLUDED
#define NET_BUFFER_H_INCLUDED

#include <netinet/in.h>

struct net_buffer {
    uint8_t * buf;
    int size;
    uint8_t * buf_ptr; /* keeps track of where to write next */
};

/* size in bytes */
int net_buffer_init(int size, struct net_buffer * initialize);

struct net_buffer * net_buffer_create(int size);

struct net_buffer * net_buffer_createFrom(uint8_t * buf, int size);

void net_buffer_delete(struct net_buffer * buf);

int net_buffer_putLong(struct net_buffer * buf, uint64_t lword);

int net_buffer_putInt(struct net_buffer * buf, uint32_t word);

int net_buffer_putShort(struct net_buffer * buf, uint16_t half);

int net_buffer_putByte(struct net_buffer * buf, uint8_t byte);

uint64_t net_buffer_getLong(struct net_buffer * buf);

uint32_t net_buffer_getInt(struct net_buffer * buf);

uint16_t net_buffer_getShort(struct net_buffer * buf);

uint8_t net_buffer_getByte(struct net_buffer * buf);

int net_buffer_copyFrom(struct net_buffer * src, void * dst, int size);

int net_buffer_copyTo(struct net_buffer * dst, void * src, int size);

int net_buffer_reset(struct net_buffer * buf);

/* 
 * @Deprecated - use sendto(sock, buf->buf, buf->size, ...)
 */
int net_buffer_send(struct net_buffer * buf, int sock, struct sockaddr_in * addr);

/* 
 * @Deprecated - use recvfrom(sock, buf->buf, buf->size, ...)
 */
int net_buffer_recv(struct net_buffer * buf, int sock, struct sockaddr_in * addr);

#endif // NET_BUFFER_H_INCLUDED
