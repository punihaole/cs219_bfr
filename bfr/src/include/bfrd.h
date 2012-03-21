#ifndef CCNUMRD_H_INCLUDED
#define CCNUMRD_H_INCLUDED

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#include "linked_list.h"
#include "content_name.h"
#include "bfrd_constants.h"

struct cluster_head {
    uint32_t nodeId;
    uint32_t clusterId;
    int distance; /* num hops to this cluster head */
    struct timespec expiration;
    struct timespec stale;
};

/* cooresponds to the bloom filter of clusters on the same level */
struct bfr_level {
    struct linked_list * clusters;
};

struct cluster {
    struct linked_list * nodes; /* only the leaf clusters have a node list! */
    struct bloom * agg_filter; /* cluster-wide aggregate filter */
    unsigned id;
    unsigned level;
};

struct node {
    struct bloom * filter;
    uint32_t nodeId;
};

/* a routing struct */
struct bfr {
    pthread_mutex_t bfr_lock;
    uint32_t nodeId;
    int num_levels;
    unsigned * clusterIds; /* stores our cluster Id on multiple levels, note: we count from 0, grid counts from 1! */
    struct cluster_head leaf_head;
	struct bitmap * is_cluster_head; /* marks if we are the cluster head for a given level */
    struct cluster leaf_cluster;
    struct bfr_level * levels; /* the top level of our cluster tree */

    /* for broadcasting our routing packets to MANET */
    struct sockaddr_in bcast_addr;
    int sock;

    double x;
    double y;

    struct node my_node; /* shortcut to my node in the tree */
};

struct listener_args {
    /* the monitor to wake up the main daemon */
    pthread_mutex_t * lock;
    pthread_cond_t  * cond;
    /* the queue to share incoming messages */
    struct synch_queue * queue;
};

extern struct bfr g_bfr;
extern struct log * g_log;

/*
 * Reads a long from a random stream. Pass in /dev/urandom or /dev/random
 * and a long * (which will be written to with a random value).
 */
int read_rand(const char * dev, long * rand);

/**
 * bfr_init
 *      Initializes our basic routing structure and grid.
 * @param levels - the number of hierarchy levels to divide the routing area.
 * @param width - the width of our networking area
 * @param height - the height of our area
 * @param startX - the starting X coord.
 * @param startY - the starting Y coord.
 **/
int bfr_init(int nodeId, unsigned levels, unsigned width, unsigned height,
               double startX, double startY);

void bfr_handle_ipc(struct listener_args * ipc_args);
void bfr_handle_net(struct listener_args * net_args);

/* returns true if I am the cluster head at given level */
bool_t amClusterHead(unsigned level);

#endif // CCNUMRD_H_INCLUDED
