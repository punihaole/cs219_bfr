#ifndef CCNU_PACKET_H_INCLUDED
#define CCNU_PACKET_H_INCLUDED

#include <netinet/in.h>

#include "ccnfd_constants.h"
#include "content_name.h"

#define PACKET_TYPE_INTEREST 0
#define PACKET_TYPE_DATA     1

/**
 * Packet formats on the wire
 *
 * Interest
 *     +packet_type = 0 : byte
 *     +ttl : byte
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

#define MIN_INTEREST_PKT_SIZE 6 /* bytes */
struct ccnf_interest_pkt {
    uint8_t ttl;
    struct content_name * name;
};

#define MIN_DATA_PKT_SIZE 18 /* bytes */
struct ccnf_data_pkt {
    uint8_t hops;
    uint32_t publisher_id;
	struct content_name * name;
	uint32_t timestamp;
	uint32_t payload_len;
	void * payload;
};

static inline void ccnf_interest_destroy(struct ccnf_interest_pkt * pkt)
{
    content_name_delete(pkt->name);
    free(pkt);
}

static inline void ccnf_data_destroy(struct ccnf_data_pkt * pkt)
{
    content_name_delete(pkt->name);
    free(pkt->payload);
    free(pkt);
}

#endif // CCNU_PACKET_H_INCLUDED
