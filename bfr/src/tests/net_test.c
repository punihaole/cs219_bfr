/* WARNING:
 * All of this is deprecated.
 * Use the scripts and binaries in apps/ to do testing.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <math.h>

#include "bfr.h"
#include "net_lib.h"

static int send_bloom(struct bloom_msg * msg)
{
    if (!msg) {
        syslog(LOG_WARNING, "send_bloom: tried to send NULL packet -- IGNORING!");
        return -1;
    }

    if (!msg->vector) {
        syslog(LOG_WARNING, "send_bloom: tried to send Bloom filter with no vector -- IGNORING!");
        return -1;
    }

    uint8_t buf[MAX_PACKET_SIZE];

    int size = 2*sizeof(uint8_t) + 3*sizeof(uint16_t) + sizeof(uint64_t);
    size += ceil(msg->vector_bits / 8.0);

    int offset = 0;
    /* pack the header */
    putByte(buf + offset, MSG_NET_BLOOMFILTER_UPDATE);
    offset += sizeof(uint8_t);
	putInt(buf+offset, 123456);
	offset += sizeof(uint32_t);
    putInt(buf + offset, size);
    offset += sizeof(uint32_t);

    /*pack the payload (bloom filter msg) */
    putByte(buf + offset, msg->origin_level);
    offset += sizeof(uint8_t);
    putShort(buf + offset, msg->origin_clusterId);
    offset += sizeof(uint16_t);
    putByte(buf + offset, msg->dest_level);
    offset += sizeof(uint8_t);
    putShort(buf + offset, msg->dest_clusterId);
    offset += sizeof(uint16_t);

    putLong(buf + offset, msg->lastHopDistance);
    offset += sizeof(uint64_t);

    putShort(buf + offset, msg->vector_bits);
    offset += sizeof(uint16_t);

    int i;
    for (i = 0; i < (ceil(msg->vector_bits / 8.0)); i++) {
        putByte(buf + offset, msg->vector[i]);
        offset += sizeof(uint8_t);
    }

	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == -1)
		perror("socket");

	int optval = 1;
	setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));
	struct sockaddr_in bcast_addr;
	memset(&bcast_addr, 0, sizeof(struct sockaddr_in));
	bcast_addr.sin_family = AF_INET;
	bcast_addr.sin_port = htons(9988);
	bcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

	int pkt_size = sizeof(struct bfr_hdr) + size;
	printf("Sending a total of %d bytes.\n", pkt_size);
/*
    if (net_sendall(buf, &pkt_size, sock, &bcast_addr) == -1) {
        printf("net_sendall: failed. sent only %d bytes\n", pkt_size);
        return -1;
    }
*/

	if (sendto(sock, buf, pkt_size, 0, (struct sockaddr * ) &bcast_addr, sizeof(bcast_addr)) == -1)
		perror("sendto");

    return 0;
}

int main()
{
	struct bloom_msg bloom;
	bloom.origin_level = 8;
	bloom.origin_clusterId = 28;
	bloom.dest_level = 9;
	bloom.dest_clusterId = 9;

	bloom.lastHopDistance = pack_ieee754_64(913.1964);

	bloom.vector_bits = 32;
	bloom.vector = (uint32_t * ) malloc(sizeof(uint32_t));
	bloom.vector[0] = 0x89;
	bloom.vector[1] = 0xab;
	bloom.vector[2] = 0xcd;
	bloom.vector[3] = 0xef;

	send_bloom(&bloom);

	return 0;
}
