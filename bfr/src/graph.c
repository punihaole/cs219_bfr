#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

#include "graph.h"

unsigned levels;
unsigned width;

struct edge {
    struct node * a;
    struct node * b;
}

struct node {
    double x;
    double y;
    unsigned id;
};

int graph_init(unsigned int levels, unsigned int width, unsigned int height, char * graph_file)
{

}

int graph_cluster(unsigned int level, double x, double y)
{

}

int graph_center(unsigned int level, unsigned int clusterId, double * x, double * y)
{

}

int graph_distance(unsigned int level, unsigned int clusterId,
                  double x, double y, double * distance)
{
}

int graph_3neighbors(unsigned level, unsigned clusterId, unsigned neighborIds[])
{

}

int graph_convertCluster(unsigned level, unsigned clusterId, unsigned other_level, unsigned * other_clusterId)
{

}
