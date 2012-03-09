#ifndef NET_LIB_H_INCLUDED
#define NET_LIB_H_INCLUDED

#include <netinet/in.h>

/* retrieve data from a buffer, transforms from network byte order to
 * host byte order */
uint8_t getByte(uint8_t * buf);

uint16_t getShort(uint8_t * buf);

uint32_t getInt(uint8_t * buf);

uint64_t getLong(uint8_t * buf);

/* pack data into a buffer, transforms from host byte order to
 * network byte order */
void putByte(uint8_t * buf, uint8_t _byte);

void putShort(uint8_t * buf, uint16_t _short);

void putInt(uint8_t * buf, uint32_t _int);

void putLong(uint8_t * buf, uint64_t _long);

/* pack or unpack doubles. Appropriately changes byte order */
uint64_t pack_ieee754_32(long double f);

uint64_t pack_ieee754_64(long double f);

float unpack_ieee754_32(uint64_t f);

double unpack_ieee754_64(uint64_t f);

/* grabs a new socket handle and sets it up for broadcast */
int broadcast_socket();

/* setups up a sockaddr to be a broadcast addr */
int broadcast_addr(struct sockaddr_in * bcast_addr, short port);

/* creates a node id from an ip addr. It iterates over the if devices present,
 * ignoring lo if
 */
uint32_t IP4_to_nodeId();

#endif // NET_LIB_H_INCLUDED
