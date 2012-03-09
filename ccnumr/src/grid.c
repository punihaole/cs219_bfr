#include "grid.h"

#include <math.h>
#include <stdlib.h>

struct grid
{
    unsigned int h;
    unsigned int w;
    unsigned int levels;
    unsigned int * divisions;
    double * cluster_height;
    double * cluster_width;
    double ** x_bounds;
    double ** y_bounds;
};

struct grid g_grid;

int grid_init(unsigned int levels, unsigned int width, unsigned int height)
{
    if (levels == 0)
        return -1;

    g_grid.h = height;
    g_grid.w = width;
    g_grid.levels = levels;

    g_grid.x_bounds = (double ** ) malloc(sizeof(double * ) * levels);
    g_grid.y_bounds = (double ** ) malloc(sizeof(double * ) * levels);

    g_grid.divisions = (unsigned int * ) malloc(sizeof(unsigned int) * levels);

    g_grid.cluster_width = (double * ) malloc(sizeof(double) * levels);
    g_grid.cluster_height = (double * ) malloc(sizeof(double) * levels);

    int i;
    for (i = 1; i <= levels; i++) {
        g_grid.divisions[i-1] = (unsigned int) pow(2, i);
        g_grid.x_bounds[i-1] = (double * ) malloc(sizeof(double) * (g_grid.divisions[i-1]));
        g_grid.y_bounds[i-1] = (double * ) malloc(sizeof(double) * (g_grid.divisions[i-1]));

        double cluster_width = (double) width / (double) g_grid.divisions[i-1];
        double cluster_height = (double )height / (double) g_grid.divisions[i-1];

        g_grid.cluster_width[i-1] = cluster_width;
        g_grid.cluster_height[i-1] = cluster_height;

        int j;
        for (j = 0; j < g_grid.divisions[i-1]; j++) {
            g_grid.x_bounds[i-1][j] = cluster_width * (j + 1);
            g_grid.y_bounds[i-1][j] = cluster_height * (j + 1);
        }
    }

    return 0;
}

int grid_cluster(unsigned level, double x, double y)
{
    if (x > g_grid.w || y > g_grid.h)
        return -1;

    if (level > g_grid.levels || level == 0)
        return -1;

    unsigned int x_tile = 0, y_tile = 0;

    while ((x_tile < g_grid.divisions[level-1]) && (x > g_grid.x_bounds[level-1][x_tile])) {
        x_tile++;
    }

    while ((y_tile < g_grid.divisions[level-1]) && (y > g_grid.y_bounds[level-1][y_tile])) {
        y_tile++;
    }

    int clusterId = x_tile + (y_tile * g_grid.divisions[level-1]);

    return clusterId;
}

static inline int num_cluster(unsigned int level)
{
    return 1 << (level * 2);
}

int grid_center(unsigned int level, unsigned int clusterId, double * x, double * y)
{
    if (level == 0 || level > g_grid.levels)
        return -1;
    if (clusterId >= num_cluster(level))
        return -1;
    if (!x || !y)
        return -1;

    unsigned int x_tile = clusterId % g_grid.divisions[level-1];
    unsigned int y_tile = clusterId / g_grid.divisions[level-1];

    *x = round(g_grid.x_bounds[level-1][x_tile] - (g_grid.cluster_width[level-1]/2.0));
    *y = round(g_grid.y_bounds[level-1][y_tile] - (g_grid.cluster_height[level-1]/2.0));

    return 0;
}

int grid_distance(unsigned int level, unsigned int clusterId,
                  double x, double y, double * distance)
{
    if (level == 0 || level > g_grid.levels)
        return -1;
    if (clusterId >= num_cluster(level))
        return -1;
    if (!distance)
        return -1;
    if (x > g_grid.x_bounds[level-1][g_grid.divisions[level-1]] || x < 0)
        return -1;
    if (y > g_grid.y_bounds[level-1][g_grid.divisions[level-1]] || y < 0)
        return -1;

    double center_x;
    double center_y;

    if (grid_center(level, clusterId, &center_x, &center_y) < 0)
        return -1;

    double delt_x = x - center_x;
    double delt_y = y - center_y;

    double x_sq = pow(delt_x, 2.0);
    double y_sq = pow(delt_y, 2.0);

    *distance = sqrt(x_sq + y_sq);

    return 0;
}

int grid_dimensions(unsigned int level,
                    double * width, double * height)
{
    if (level == 0 || level > g_grid.levels)
        return -1;
    if (!width || !height)
        return -1;

    *width = g_grid.cluster_width[level - 1];
    *height = g_grid.cluster_height[level - 1];

    return 0;
}
