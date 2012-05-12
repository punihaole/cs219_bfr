#ifndef CCNU_PACKET_H_INCLUDED
#define CCNU_PACKET_H_INCLUDED

#include <netinet/in.h>

#include "ccnud_constants.h"
#include "content_name.h"

#define PACKET_TYPE_INTEREST 0
#define PACKET_TYPE_DATA     1

/**
 * Packet formats on the wire
 * All fields are in host byte order.
 *
 * Interest
 *     +packet_type = 0 : byte
 *     +nonce : short
 *     +ttl : byte
 *     +orig_level : byte
 *     +orig_clusterId : short
 *     +dest_level : byte
 *     +dest_clusterId : short
 *     +distance : long
 *     +name_len : int
 *     +name : char[name_len]
 *
 * Data
 *     +packet_type = 1 : byte
 *     +hops : byte
 *     +publisher_id : int
 *     +name_len : int
 *     +name : char[name_len]
 *     +timestamp : int
 *     +size : int
 *     +payload : byte[size]
 **/

#define MIN_INTEREST_PKT_SIZE 22 /* bytes */
struct ccnu_interest_pkt {
    uint16_t nonce;
    uint8_t ttl;
    uint8_t orig_level;
    uint16_t orig_clusterId;
    uint8_t dest_level;
    uint16_t dest_clusterId;
    uint64_t distance; /* IEEE 754 formatted */

    struct content_name * name;
};

static inline void ccnu_interest_destroy(struct ccnu_interest_pkt * pkt)
{
    content_name_delete(pkt->name);
    pkt->name = NULL;
    free(pkt);
}

#define MIN_DATA_PKT_SIZE 18 /* bytes */
struct ccnu_data_pkt {
    uint8_t hops;
    uint32_t publisher_id;
	struct content_name * name;
	uint32_t timestamp;
	uint32_t payload_len;
	void * payload;
};

static inline void ccnu_data_destroy(struct ccnu_data_pkt * pkt)
{
    content_name_delete(pkt->name);
    pkt->name = NULL;
    free(pkt->payload);
    pkt->payload = NULL;
    free(pkt);
}

#endif // CCNU_PACKET_H_INCLUDED
