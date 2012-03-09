#ifndef CRUSTD_CONSTANTS_H_INCLUDED
#define CRUSTD_CONSTANTS_H_INCLUDED

#include "constants.h"

/* IPC */
#define SOCK_QUEUE 5
#define LISTEN_PORT 9988
#define DEFAULT_NODE_ID IP4_to_nodeId()

/* GRID */
#define DEFAULT_NUM_LEVELS 2
#define DEFAULT_GRID_WIDTH 1000
#define DEFAULT_GRID_HEIGHT 1000
#define DEFAULT_GRID_STARTX 0
#define DEFAULT_GRID_STARTY 0

/* DISTANCE TABLE */
#define DEFAULT_DIST_TABLE_SIZE 20
#define MAX_HOPS 10

/* INTERVALS */
/* how long to consider a cluster head as fresh, i.e. how long to keep sharing
 * that node as the cluster head.
 */
#define CLUSTER_HEAD_FRESHNESS_PERIOD_SEC 30
/* how long before forgetting a cluster head altogether. */
#define CLUSTER_HEAD_EXPIRATION_PERIOD_SEC 45
#define DEFAULT_CLUSTER_INTERVAL_SEC 45
#define DEFAULT_BLOOM_INTERVAL_SEC 60
#define DEFAULT_BACKOFF_INTERVAL_MS 33
#define RESPONSE_MAX_WAIT_TIME_MS 100

/* BLOOM/SUMMARY */
#define BLOOM_ARGS 5, elfhash, sdbmhash, djbhash, dekhash, bphash

#define HASH(n) (elfhash(n))

/* MISC */
#ifndef max
	#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
	#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef msleep
    #define msleep(n) (usleep((n) * 1000))
#endif

#endif // CRUSTD_CONSTANTS_H_INCLUDED
