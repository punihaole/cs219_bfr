#include <math.h>

#include "cluster.h"
#include "bloom_filter.h"
#include "log.h"
#include "grid.h"

extern struct log * g_log;

int clus_get_level(unsigned level_id, struct bfr_level ** level)
{
    if (level_id > g_bfr.num_levels || (level_id == 0))
        return -1;

    *level = &g_bfr.levels[level_id-1];
    return 0;
}

int clus_get_cluster(unsigned level_id, unsigned cluster_id, struct cluster ** clus)
{
    if ((level_id > g_bfr.num_levels) || (level_id == 0))
        return -1;
    if ((level_id == g_bfr.num_levels) && (cluster_id == clus_get_clusterId(level_id))) {
        *clus = &g_bfr.leaf_cluster;
        return 0;
    }

    struct bfr_level * level = &g_bfr.levels[level_id - 1];
    int i;
    for (i = 0; i < level->clusters->len; i++) {
        struct cluster * c = NULL;
        c = (struct cluster * ) linked_list_get(level->clusters, i);
        if (c->id == cluster_id) {
            *clus = c;
            return 0;
        }
    }

    return -1;
}

int clus_add_cluster(struct cluster * clus)
{
    struct bfr_level * level = NULL;

    if (clus_get_level(clus->level, &level) != 0) return -1;
    if (!level) return -1;

    linked_list_append(level->clusters, clus);
    return 0;
}

int clus_get_node(unsigned cluster_id, uint32_t node_id, struct node ** _node)
{
    if (cluster_id == clus_leaf_clusterId()) {
        struct cluster * c = &g_bfr.leaf_cluster;
        int j;
        for (j = 0; j < c->nodes->len; j++) {
            struct node * n = (struct node * ) linked_list_get(c->nodes, j);
            if (!n)
                continue;
            if (n->nodeId == node_id) {
                *_node = n;
                return 0;
            }
        }
    }
    struct bfr_level * level = &g_bfr.levels[g_bfr.num_levels-1];

    int i;
    for (i = 0; i < level->clusters->len; i++) {
        struct cluster * c = (struct cluster * ) linked_list_get(level->clusters, i);
        if (c->id == cluster_id) {
            int j;
            if (!c->nodes) return -1;

            for (j = 0; j < c->nodes->len; j++) {
                struct node * n = (struct node * ) linked_list_get(c->nodes, j);
                if (!n)
                    continue;
                if (n->nodeId == node_id) {
                    *_node = n;
                    return 0;
                }
            }
        }
    }

    return -1;
}

int clus_add_node(unsigned cluster_id, struct node * _node)
{
    if (cluster_id == clus_leaf_clusterId()) {
        struct cluster * c = &g_bfr.leaf_cluster;
        linked_list_append(c->nodes, _node);
    }

    struct bfr_level * level = &g_bfr.levels[g_bfr.num_levels-1];

    int i;
    for (i = 0; i < level->clusters->len; i++) {
        struct cluster * c = (struct cluster * ) linked_list_get(level->clusters, i);
        if (c->id == cluster_id) {
            linked_list_append(c->nodes, _node);
            return 0;
        }
    }

    return -1;
}

int clus_compute_aggregate(struct cluster * clus)
{
    if (!clus)
        return -1;

    if (!clus->agg_filter)
        return -2;

    /* first clear the agg_filter */
    memset(clus->agg_filter->vector->map, 0, clus->agg_filter->vector->num_words);

    int i;
    if ((clus->level == g_bfr.num_levels) && (clus->id == clus_leaf_clusterId())) {
        /* leaf aggregates are calculated from the individual nodes' BFs */
        for (i = 0; i < clus->nodes->len; i++) {
            struct node * _node = (struct node * ) linked_list_get(clus->nodes, i);
            if (!_node || !_node->filter)
                continue;
            bloom_or(clus->agg_filter, _node->filter, clus->agg_filter);
        }
    } else {
        /* aggregate is calculated from other aggregates */
        struct bfr_level * level = NULL;
        if (clus_get_level(clus->level, &level) < 0) return -1;

        for (i = 0; i < level->clusters->len; i++) {
            unsigned converted_clusterId;
            struct cluster * agg_clus = linked_list_get(level->clusters, i);
            if (grid_convertCluster(agg_clus->level, agg_clus->id, clus->level, &converted_clusterId) < 0)
                continue;
            if (converted_clusterId != clus->id)
                continue;
            bloom_or(clus->agg_filter, agg_clus->agg_filter, clus->agg_filter);
        }
    }

    return 0;
}

unsigned clus_get_clusterId(unsigned level)
{
    if (level > g_bfr.num_levels)
        level = g_bfr.num_levels;

    return g_bfr.clusterIds[level - 1];
}

unsigned clus_get_priorityheadCluster(unsigned level)
{
    if (level > g_bfr.num_levels)
        level = g_bfr.num_levels;
    if (level == 1) {
        /* special case, just return my clusterId */
        return clus_get_clusterId(level);
    }

    unsigned head;
    unsigned my_clusterId = clus_get_clusterId(level);
    if (my_clusterId % 2 == 1)
        my_clusterId--;

    if (((unsigned)(my_clusterId / (floor(4 * (level - 1)))) % 2) == 1)
        head = my_clusterId - (4 * (level - 1));
    else
        head = my_clusterId;

    return head;
}

void clus_destroy_cluster(struct cluster * clus)
{
    if (!clus) return;

    linked_list_delete(clus->nodes);
    bloom_destroy(clus->agg_filter);
    free(clus);
    clus = NULL;
}

void clus_destroy_node(struct node * _node)
{
    if (!_node) return;

    bloom_destroy(_node->filter);
    free(_node);
    _node = NULL;
}

static int search_bloom(struct bloom * filter, struct content_name * name)
{
    if (!filter) return 0;
    /* we iteratively check the content name in the bloom filter */
    char str[MAX_NAME_LENGTH];
    str[0] = '\0';

    int matches = 0;
    struct component * curr = name->head;
    /* no longer using the content_name_getComponent func. Doing this by hand
     * is more flexible
     */
    char * copyFrom;
    copyFrom = name->full_name;
    int i;
    for (i = 1; i <= name->num_components; i++) {
        strncat(str, copyFrom, curr->len+1);
        copyFrom += curr->len + 1;
        if (bloom_check(filter, str) == 1)
            matches = i;
        curr = curr->next;
    }

    return matches; /* return num component matches */
}

/* searches the cluster bloom filters for the level and cluster Id for content */
int clus_findCluster(struct content_name * name, unsigned * level, unsigned * clusterId)
{
    if (!name || !level || !clusterId)
        return -1;

    int i;
    int longest_match = 0; /* # of longest components */
    unsigned lm_level = g_bfr.num_levels; /* the level and cluster ID cooresponding to longest match */
    unsigned lm_clusterId = g_bfr.clusterIds[lm_level - 1];

    for (i = g_bfr.num_levels-1; i >= 0; i--) {
        log_print(g_log, "level = %d, clusters = %d", i+1, g_bfr.levels[i].clusters->len);

        int j;
        for (j = 0; j < g_bfr.levels[i].clusters->len; j++) {
            struct cluster * c;
            c = (struct cluster * ) linked_list_get(g_bfr.levels[i].clusters, j);

            int matches;
            struct bloom * filter = c->agg_filter;
            if (!filter) continue;

            log_print(g_log, "cluster = %d:%d, searching aggregate filter", c->level, c->id);
            if ((matches = search_bloom(filter, name)) > longest_match) {
                log_print(g_log, "cluster: found matches = %d", matches);
                lm_level = i+1;
                lm_clusterId = c->id;
                longest_match = matches;
            }
        }
    }

    if (longest_match > 0) {
        *level = lm_level;
        *clusterId = lm_clusterId;
        log_print(g_log, "decided: found match, fwd to (%u:%u)", *level, *clusterId);
    } else {
        log_print(g_log, "decided: match not found");
        return -1;
    }

    return 0;
}
