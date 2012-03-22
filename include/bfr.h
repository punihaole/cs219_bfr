/**
 * bfr.h
 *
 * Useful functions for clients to communicate to the daemon.
 * In general, we need to keep our location fairly up to date.
 * Whenever we move a lot we should send a loc_msg.
 *
 * The sendMsg function will send a provided msg to the daemon's domain
 * listening socket (provided the msg is properly formatted and the
 * daemon is already running).
 *
 * For now the only implemented IPC message is loc_msg.
 *
 **/

#ifndef CRUST_H_INCLUDED
#define CRUST_H_INCLUDED

#include <netinet/in.h>

#define MSG_IPC_LOCATION_UPDATE     0
#define MSG_IPC_INTEREST_FWD_QUERY  1 /* query to forward an interest */
#define MSG_IPC_INTEREST_DEST_QUERY 2 /*query where to forward an interest */
#define MSG_IPC_DISTANCE_UPDATE     4 /*updates our distance table */

#define MSG_NET_CLUSTER_JOIN       1
#define MSG_NET_CLUSTER_RESPONSE   2
#define MSG_NET_BLOOMFILTER_UPDATE 3
#define MSG_NET_SLEEPING_PILL      4

#define BFR_MAX_PACKET_SIZE (1500 - 8 - 12) /* UDP+IP overhead of 20 bytes */

#define HDR_SIZE (sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t))
struct bfr_hdr {
    uint8_t type;
    uint32_t nodeId;
    uint32_t payload_size;
};

struct bfr_payload
{
    uint8_t * data;
};

struct bfr_msg {
    struct bfr_hdr hdr;
    struct bfr_payload payload;
};

#define BLOOM_MSG_MIN_SIZE (sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint64_t) + sizeof(uint16_t))
struct bloom_msg {
    uint8_t origin_level;
    uint16_t origin_clusterId;
    uint8_t dest_level;
    uint16_t dest_clusterId;

    uint64_t lastHopDistance; /* ieee754 encoded double */

    uint16_t vector_bits; /* size of bloomfilter in bits */
    uint32_t * vector; /* the bloomfilter in a byte-aligned bit array */
};

#define SLEEPING_PILL_MSG_SIZE (sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint8_t))
struct sleeping_pill_msg {
	uint8_t level;
	uint16_t clusterId;
	uint32_t clusterHead;
	uint8_t hopCount;
};

#define LOC_MSG_SIZE (2*sizeof(uint64_t))

struct loc_msg {
    uint64_t x; /* IEEE 754 */
    uint64_t y; /* IEEE 754 */
};

#define CLUSTER_JOIN_MSG_SIZE (sizeof(uint8_t) + sizeof(uint16_t))

struct cluster_join_msg {
    uint8_t cluster_level;
    uint16_t cluster_id;
};

#define CLUSTER_RESPONSE_MSG_SIZE (sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint8_t))

struct cluster_msg {
    uint8_t cluster_level;
    uint16_t cluster_id;

    uint32_t cluster_head;
	uint8_t hops;
};

struct interest_query_msg {
    int name_len;
    struct content_name * name;
    int orig_level;
    int orig_clusterId;
    int dest_level;
    int dest_clusterId;
    uint64_t last_hop_dist;
};

inline void bfr_did2sockpath(uint32_t daemonId, char * str, int size);

void bfr_msg_delete(struct bfr_msg * msg);

/*
 * Sends a message to the daemon's domain socket.
 * !Deprecated! - use the specialized functions for passing messages to the
 * daemon.
 * Some of the tests still use this message so I will keep it.
 */
int bfr_sendMsg(struct bfr_msg * msg);

/**
 * bfr_sendLoc
 * Sends a message to the daemon's domain socket to update the geo-location.
 * Returns 0 on success.
 *
 **/
int bfr_sendLoc(double x, double y);

/**
 * bfr_sendQuery
 * Sends a message to the daemon's domain socket for a query to forward an
 * interest.
 * Returns 0 on success, -1 on failure to send.
 *
 * If the function returns successfully the values pointed to by dest_level and
 * dest_clusterId will be updated if necessary and last_hop_dist will be updated
 * with the new distance to destination cluster. Also, the value of need_fwd
 * will be set to 1 if the interest should be forwarded and 0 otherwise.
 *
 **/
int bfr_sendQry(struct content_name * name,
                  unsigned * orig_level, unsigned * orig_clusterId,
                  unsigned * dest_level, unsigned * dest_clusterId,
                  double * last_hop_dist, int * need_fwd);

/**
 * bfr_sendWhere
 * Sends a query to the daemon's domain socket. This query is interepreted as
 * a request for the destination cluster ID and destination level for an
 * interest cooresponding to the content name provided. The ccnu daemon will
 * use this function for satisfying application interest's for data not in the
 * CS. This query will be useful for gettign the information required to
 * populate the interest fields used for routing.
 *
 **/
int bfr_sendWhere(struct content_name * name,
                    unsigned * orig_level, unsigned * orig_clusterId,
                    unsigned * dest_level, unsigned * dest_cluster,
                    double * distance);

/**
 * bfr_sendDistance
 * When we recv a data packet that is local we send the number of hops the data packet
 * has traversed to the routing daemon. This allows us to update the distance table.
 **/
int bfr_sendDistance(struct content_name * name, int hops);
#endif // CRUST_H_INCLUDED
