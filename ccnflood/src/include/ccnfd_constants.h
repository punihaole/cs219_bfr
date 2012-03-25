#ifndef CCNUD_CONSTANTS_H_INCLUDED
#define CCNUD_CONSTANTS_H_INCLUDED

/* CS */
#define CS_SIZE 500
#define DEFAULT_P 0.02 /* false positive probability */

/* PIT */
#define PIT_SIZE 500
#define PIT_LIFETIME_MS 3000

/* NET */
#define DEFAULT_NODE_ID IP4_to_nodeId()
#define CCNF_MAX_PACKET_SIZE (1500 - 8 - 12) /* UDP+IP overhead of 20 bytes */
#define MAX_TTL 6
#define LISTEN_PORT 8282
#define SOCK_QUEUE 5

#define CCNU_USE_SLIDING_WINDOW
typedef enum {
    SLOW_START,
    CONG_AVOID
} cc_state;

#define INTEREST_FLOWS 3
#define DEFAULT_INTEREST_PIPELINE 50
#define MAX_INTEREST_PIPELINE (PIT_SIZE - 25)
#define INTEREST_TIMEOUT_MS 1500 /* ms */
#define INTEREST_MAX_ATTEMPTS 5
#define MAX_HOPS 3 /*number of hops taken in cluster fwding */

#define INTEREST_MAX_PATHS 5 /* number of paths 1 interest can take */

#endif // CCNUD_CONSTANTS_H_INCLUDED
