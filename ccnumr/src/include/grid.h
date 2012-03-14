/**
 * grid.h
 *
 * Used for managing the grid. We can map an (x,y) choord. to a cluster ID.
 * The grid is a hierarchially arranged space like so:
 * ------------------------  ------------------------
 * |           |          |  |           |          |
 * |           |          |  |           |          |
 * |           |          |  |     2     |     3    |
 * |  8     9  |  ...     |  |           |          |
 * |___________|__________|  |___________|__________|
 * |     |     |          |  |           |          |
 * |  4  |  5  |  6   7   |  |           |          |
 * |_____|_____|          |  |     0     |     1    |
 * |     |     |          |  |           |          |
 * |  0  |  1  |  2   3   |  |           |          |
 * ------------------------  ------------------------
 *  level 2                   level 1
 *
 * Not that we are not restricting the number of levels of subdivision. But
 * there must be at least 1 (that is broken up into quadrants).
 **/

#ifndef GRID_H_INCLUDED
#define GRID_H_INCLUDED

struct grid;

/**
 * grid_init
 * Called by higher level init functions. For the sake of reusability this
 * function is separated out into a separate module.
 *
 **/
int grid_init(unsigned int levels, unsigned int width, unsigned int height);

/**
 * grid_cluster
 * Maps (x,y) coords to a cluster ID for a given level. The specified level
 * must be <= the number of levels given when grid_init was called.
 * This allows us to recursively calculate our cluster based on what level
 * we are interested in. Level 1 cooresponds to the quadrants clusters,
 * level 2 is the 16 clusters, level 3 is the 64 clusters, etc...
 */
int grid_cluster(unsigned int level, double x, double y);

/**
 * grid_center
 * Calculates the center of a grid cluster.
 * @param level - the grid layer
 * @param clusterId - the cluster ID for the given grid layer
 * @param x, y - the positions of the center of the cluster. The values stored
 * in x and y will be overwritten.
 * returns 0 on success.
 */
int grid_center(unsigned int level, unsigned int clusterId, double * x, double * y);

/**
 * grid_distance
 * Calculates the distance from a given x,y to the center of a given cluster.
 * @param level - the grid layer
 * @param clusterId - the cluster ID for the given grid layer
 * @param x, y - the positions to calculate the distance from
 * @param distance - the calcualted distance, old value will be overwritten
 * returns 0 on success
 */
int grid_distance(unsigned int level, unsigned int clusterId,
                  double x, double y, double * distance);

/**
 * grid_dimensions
 * Calculates the dimensions of clusters at a given level.
 * @param level - the grid layer level
 * @param width - overwrites with the width of the cluster.
 * @param height - overwrites with the height of the cluster.
 */
int grid_dimensions(unsigned int level,
                    double * width, double * height);

/**
 * grid_3neighbors
 * Calculates our 3 neighbors in our sub-quadrant at whatever level.
 * neighborIds is a list of unsigned ints of size 3!
 **/
int grid_3neighbors(unsigned level, unsigned clusterId, unsigned neighborIds[]);

#endif // GRID_H_INCLUDED
