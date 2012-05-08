#ifndef CRUSTD_CONSTANTS_H_INCLUDED
#define CRUSTD_CONSTANTS_H_INCLUDED

#include "constants.h"

/* STRATEGY */
#define MAX_INTERFACES 5
#define DEFAULT_HANDLER_POOL 5
#define BFR_ETHER_PROTO 0x88b6

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
#define DEFAULT_CLUSTER_INTERVAL_SEC 20
#define DEFAULT_BLOOM_INTERVAL_SEC 20
#define DEFAULT_BACKOFF_INTERVAL_MS 33
#define RESPONSE_MAX_WAIT_TIME_MS 100

#endif // CRUSTD_CONSTANTS_H_INCLUDED
