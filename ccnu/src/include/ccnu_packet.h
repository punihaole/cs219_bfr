#ifndef CCNU_PACKET_H_INCLUDED
#define CCNU_PACKET_H_INCLUDED

#include <netinet/in.h>

#include "ccnud_constants.h"
#include "content_name.h"

#define PACKET_TYPE_INTEREST 0
#define PACKET_TYPE_DATA     1

/**
 * Packet formats on the wire
 *
 * Interest
 *     +packet_type = 0 : byte
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

#define MIN_INTEREST_PKT_SIZE 20 /* bytes */
struct ccnu_interest_pkt {
    uint8_t ttl;
    uint8_t orig_level;
    uint16_t orig_clusterId;
    uint8_t dest_level;
    uint16_t dest_clusterId;
    uint64_t distance; /* IEEE 754 formatted */

    struct content_name * name;
};

#define MIN_DATA_PKT_SIZE 18 /* bytes */
struct ccnu_data_pkt {
    uint8_t hops;
    uint32_t publisher_id;
	struct content_name * name;
	uint32_t timestamp;
	uint32_t payload_len;
	void * payload;
};

#endif // CCNU_PACKET_H_INCLUDED
