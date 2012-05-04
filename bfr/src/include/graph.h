#ifndef GRAPH_H_INCLUDED
#define GRAPH_H_INCLUDED

/**
 * grid_init
 * Called by higher level init functions. For the sake of reusability this
 * function is separated out into a separate module.
 *
 **/
int graph_init(unsigned int levels, unsigned int width, unsigned int height, char * graph_file);

/**
 * graph_cluster
 * Maps (x,y) coords to a cluster ID for a given level. The specified level
 * must be <= the number of levels given when grid_init was called.
 * This allows us to recursively calculate our cluster based on what level
 * we are interested in. Level 1 cooresponds to the quadrants clusters,
 * level 2 is the 16 clusters, level 3 is the 64 clusters, etc...
 */
int graph_cluster(unsigned int level, double x, double y);

/**
 * graph_center
 * Calculates the center of a grid cluster.
 * @param level - the grid layer
 * @param clusterId - the cluster ID for the given grid layer
 * @param x, y - the positions of the center of the cluster. The values stored
 * in x and y will be overwritten.
 * returns 0 on success.
 */
int graph_center(unsigned int level, unsigned int clusterId, double * x, double * y);

/**
 * graph_distance
 * Calculates the distance from a given x,y to the center of a given cluster.
 * @param level - the grid layer
 * @param clusterId - the cluster ID for the given grid layer
 * @param x, y - the positions to calculate the distance from
 * @param distance - the calcualted distance, old value will be overwritten
 * returns 0 on success
 */
int graph_distance(unsigned int level, unsigned int clusterId,
                  double x, double y, double * distance);

/**
 * grid_3neighbors
 * Calculates our 3 neighbors in our sub-quadrant at whatever level.
 * neighborIds is an array of unsigned ints of size 3!
 **/
int graph_3neighbors(unsigned level, unsigned clusterId, unsigned neighborIds[]);

/*
 * Converts a cluster ID to another clusterID.
 * I.e Converts 2:0, 2:1, 2:4, 2:5 to 1:0 if we set other_level to 1.
 * other_clusterId, overwritten by the calculated value if rv = 0.
 */
int graph_convertCluster(unsigned level, unsigned clusterId, unsigned other_level, unsigned * other_clusterId);

#endif // GRAPH_H_INCLUDED
