#ifndef CLUSTER_H_INCLUDED
#define CLUSTER_H_INCLUDED

#include "bfrd.h"

/* lock the g_bfr struct before touching any of these functions! */

#define clus_leaf_clusterId() clus_get_clusterId(g_bfr.num_levels)

int clus_get_level(unsigned level_id, struct bfr_level ** level);

int clus_get_cluster(unsigned level_id, unsigned cluster_id, struct cluster ** clus);

int clus_add_cluster(struct cluster * clus);

int clus_get_node(unsigned cluster_id, uint32_t node_id, struct node ** _node);

/* clears and recomputes the aggregate bloom filter for that cluster */
int clus_compute_aggregate(struct cluster * clus);

/* get our cluster Id on a given level */
unsigned clus_get_clusterId(unsigned level);

/* returns the leaf cluster id where the cluster head for the given level resides */
unsigned clus_get_clusterHead(unsigned level);

void clus_destroy_cluster(struct cluster * clus);

void clus_destroy_node(struct node * _node);

int clus_findCluster(struct content_name * name, unsigned * level, unsigned * clusterId);

#endif // CLUSTER_H_INCLUDED
