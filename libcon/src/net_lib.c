#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>

#include "net_lib.h"

uint8_t getByte(uint8_t * buf)
{
    uint8_t _byte;
    memcpy(&_byte, buf, sizeof(uint8_t));

    return _byte;
}

uint16_t getShort(uint8_t * buf)
{
    uint16_t _short;
    memcpy(&_short, buf, sizeof(uint16_t));

    return ntohs(_short);
}

uint32_t getInt(uint8_t * buf)
{
    uint32_t _int;
    memcpy(&_int, buf, sizeof(uint32_t));

    return ntohl(_int);
}

uint64_t getLong(uint8_t * buf)
{
    uint32_t upper;
    uint32_t lower;

    memcpy(&upper, buf, sizeof(uint32_t));
    memcpy(&lower, buf+sizeof(uint32_t), sizeof(uint32_t));

    upper = ntohl(upper);
    lower = ntohl(lower);

    uint64_t result = lower;
    result += ((long long) upper) << 32;

    return result;
}

void putByte(uint8_t * buf, uint8_t _byte)
{
    /* a byte has no mis-ordering just throw it in the buf */
    memcpy(buf, &_byte, sizeof(uint8_t));
}

void putShort(uint8_t * buf, uint16_t _short)
{
    uint16_t to_net = htons(_short);
    memcpy(buf, &to_net, sizeof(uint16_t));
}

void putInt(uint8_t * buf, uint32_t _int)
{
    uint32_t to_net = htonl(_int);
    memcpy(buf, &to_net, sizeof(uint32_t));
}

void putLong(uint8_t * buf, uint64_t _long)
{
    uint32_t upper;
    uint32_t lower;

    memcpy(&lower, &_long, sizeof(uint32_t));
    _long = _long >> 32;
    memcpy(&upper, &_long, sizeof(uint32_t));

    upper = htonl(upper);
    lower = htonl(lower);

    memcpy(buf, &upper, sizeof(uint32_t));
    memcpy(buf+sizeof(uint32_t), &lower, sizeof(uint32_t));
}

/* from beej networking programming guide http://beej.us/guide/bgnet/ */
static uint64_t pack_ieee754(long double f, unsigned int bits, unsigned int expbits)
{
    long double fnorm;
    int shift;
    long long sign, exp, significand;
    unsigned int significandbits = bits - expbits - 1; // -1 for sign bit

    if (f == 0.0) return 0; // get this special case out of the way

    // check sign and begin normalization
    if (f < 0) { sign = 1; fnorm = -f; }
    else { sign = 0; fnorm = f; }

    // get the normalized form of f and track the exponent
    shift = 0;
    while(fnorm >= 2.0) { fnorm /= 2.0; shift++; }
    while(fnorm < 1.0) { fnorm *= 2.0; shift--; }
    fnorm = fnorm - 1.0;

    // calculate the binary form (non-float) of the significand data
    significand = fnorm * ((1LL<<significandbits) + 0.5f);

    // get the biased exponent
    exp = shift + ((1<<(expbits-1)) - 1); // shift + bias

    // return the final answer
    return (sign<<(bits-1)) | (exp<<(bits-expbits-1)) | significand;
}

/* from beej networking programming guide http://beej.us/guide/bgnet/ */
static long double unpack_ieee754(uint64_t i, unsigned int bits, unsigned int expbits)
{
    long double result;
    long long shift;
    unsigned int bias;
    unsigned int significandbits = bits - expbits - 1; // -1 for sign bit

    if (i == 0) return 0.0;

    // pull the significand
    result = (i&((1LL<<significandbits)-1)); // mask
    result /= (1LL<<significandbits); // convert back to float
    result += 1.0f; // add the one back on

    // deal with the exponent
    bias = (1<<(expbits-1)) - 1;
    shift = ((i>>significandbits)&((1LL<<expbits)-1)) - bias;
    while(shift > 0) { result *= 2.0; shift--; }
    while(shift < 0) { result /= 2.0; shift++; }

    // sign it
    result *= (i>>(bits-1))&1? -1.0: 1.0;

    return result;
}

uint64_t pack_ieee754_32(long double f)
{
    return pack_ieee754(f, 32, 8);
}

uint64_t pack_ieee754_64(long double f)
{
    return pack_ieee754(f, 64, 11);
}

float unpack_ieee754_32(uint64_t f)
{
    return unpack_ieee754(f, 32, 8);
}

double unpack_ieee754_64(uint64_t f)
{
    return unpack_ieee754(f, 64, 11);
}

int broadcast_socket()
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == -1) {
        syslog(LOG_ERR, "broadcast_socket: failed - %s.", strerror(errno));
        return -1;
	}

    /* set the socket to broadcast */
	int optval = 1;
	setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	return sock;
}

int broadcast_addr(struct sockaddr_in * bcast_addr, short port)
{
    if (!bcast_addr) {
        syslog(LOG_WARNING, "broadcast_addr: bcast_addr not allocated!");
        return -1;
    }

	memset(bcast_addr, 0, sizeof(struct sockaddr_in));
	bcast_addr->sin_family = AF_INET;
	bcast_addr->sin_port = htons(port);
	bcast_addr->sin_addr.s_addr = htonl(INADDR_BROADCAST);
	return 0;
}

uint32_t IP4_to_nodeId()
{
    struct ifaddrs * myaddrs, * ifa;
    void * in_addr;
    char str[INET_ADDRSTRLEN];
    const uint32_t lo_ip = 2130706433;
    uint32_t can_ip = 0;

    if (getifaddrs(&myaddrs) != 0) {
        syslog(LOG_ERR, "IP4_to_nodeId: failed to set node ID - %s.", strerror(errno));
        exit(EXIT_FAILURE);
    }

    for (ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;
        if (!(ifa->ifa_flags && IFF_UP))
            continue;

        switch (ifa->ifa_addr->sa_family) {
            case AF_INET:
            {
                struct sockaddr_in * ipv4 = (struct sockaddr_in *) ifa->ifa_addr;
                in_addr = &ipv4->sin_addr;
                break;
            }
            case AF_INET6: /* skip ipv6 addresses */
                continue;
            default:
                continue;
        }

        if (!inet_ntop(ifa->ifa_addr->sa_family, in_addr, str, sizeof(str))) {
            syslog(LOG_WARNING, "IP4_to_nodeId: failed to extract IP addr? - %s.",
                   strerror(errno));
            syslog(LOG_WARNING, "IP4_to_nodeId: %s.", ifa->ifa_name);
            exit(EXIT_FAILURE);
        } else {
            //syslog(LOG_INFO, "IP4_to_nodeId: found IP addr -%s: %s.", ifa->ifa_name, str);

            unsigned oct_0, oct_1, oct_2, oct_3;
            uint32_t ip = 0;

            if (sscanf(str, "%d.%d.%d.%d", &oct_3, &oct_2, &oct_1, &oct_0) != 4) {
                syslog(LOG_WARNING, "IP4_to_nodeId: failed to extract octets? - %s.",
                   strerror(errno));
                exit(EXIT_FAILURE);
            }

            ip = oct_3 << 24;
            ip += oct_2 << 16;
            ip += oct_1 << 8;
            ip += oct_0;

            if (ip != lo_ip)
                can_ip = ip;
        }
    }
    freeifaddrs(myaddrs);
    return can_ip;
}
