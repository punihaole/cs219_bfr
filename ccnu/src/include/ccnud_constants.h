#ifndef CCNUD_CONSTANTS_H_INCLUDED
#define CCNUD_CONSTANTS_H_INCLUDED

/* CS */
#define CS_SIZE 65536
#define DEFAULT_P 0.02 /* false positive probability */
#define BLOOM_ARGS 5, elfhash, sdbmhash, djbhash, dekhash, bphash

/* PIT */
#define PIT_SIZE 50
#define PIT_LIFETIME_MS 5000

/* NET */
#define DEFAULT_NODE_ID IP4_to_nodeId()
#define CCNU_MAX_PACKET_SIZE 1500
#define MAX_TTL 6
#define LISTEN_PORT 8282
#define SOCK_QUEUE 5

#define INTEREST_TIMEOUT_MS 1000 /* ms */
#define INTEREST_MAX_ATTEMPTS 5
#define MAX_HOPS 10 /*number of hops taken in cluster fwding */

#endif // CCNUD_CONSTANTS_H_INCLUDED
