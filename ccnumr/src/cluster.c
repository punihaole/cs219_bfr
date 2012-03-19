#include <math.h>

#include "cluster.h"
#include "bloom_filter.h"
#include "log.h"

extern struct log * g_log;

int clus_get_level(unsigned level_id, struct ccnumr_level ** level)
{
    if (level_id > g_ccnumr.num_levels && level_id != 0)
        return -1;

    *level = &g_ccnumr.levels[level_id-1];
    return 0;
}

int clus_get_cluster(unsigned level_id, unsigned cluster_id, struct cluster ** clus)
{
    if (level_id > g_ccnumr.num_levels && level_id != 0)
        return -1;

    struct ccnumr_level * level = &g_ccnumr.levels[level_id - 1];
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
    struct ccnumr_level * level = NULL;

    if (clus_get_level(clus->level, &level) != 0) return -1;
    if (!level) return -1;

    linked_list_append(level->clusters, clus);
    return 0;
}

int clus_get_node(unsigned cluster_id, uint32_t node_id, struct node ** _node)
{
    struct ccnumr_level * level = &g_ccnumr.levels[g_ccnumr.num_levels-1];

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

int clus_compute_aggregate(struct cluster * clus)
{
    if (!clus)
        return -1;

    if (!clus->agg_filter)
        return -2;

    /* first clear the agg_filter */
    memset(clus->agg_filter->vector->map, 0, clus->agg_filter->vector->num_words);

    int i;
    for (i = 0; i < clus->nodes->len; i++) {
        struct node * _node = (struct node * ) linked_list_get(clus->nodes, i);
        if (!_node || !_node->filter)
            continue;
        bloom_or(clus->agg_filter, _node->filter, clus->agg_filter);
    }

    return 0;
}

unsigned clus_get_clusterId(unsigned level)
{
    if (level > g_ccnumr.num_levels)
        level = g_ccnumr.num_levels;

    return g_ccnumr.clusterIds[level - 1];
}

unsigned clus_get_clusterHead(unsigned level)
{
    if (level > g_ccnumr.num_levels)
        level = g_ccnumr.num_levels;
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
    /* we iteratively check the content name in the bloom filter */
    char str[MAX_NAME_LENGTH];
    str[0] = '\0';

    int i = 1, matches = 0;
    struct component * curr = name->head;
    /* no longer using the content_name_getComponent func. Doing this by hand
     * is more flexible
     */
    char * copyFrom;
    copyFrom = name->full_name;
    while (curr != NULL) {
        strncat(str, copyFrom, curr->len+1);
        copyFrom += curr->len + 1;
        if (bloom_check(filter, str) == 1)
            matches = i;
        i++;
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
    unsigned lm_level = g_ccnumr.num_levels; /* the level and cluster ID cooresponding to longest match */
    unsigned lm_clusterId = g_ccnumr.clusterIds[lm_level - 1];

    for (i = 0; i < g_ccnumr.num_levels; i++) {
        log_print(g_log, "level = %d, clusters = %d", i, g_ccnumr.levels[i].clusters->len);

        int j;
        for (j = 0; j < g_ccnumr.levels[i].clusters->len; j++) {
            struct cluster * c;
            c = (struct cluster * ) linked_list_get(g_ccnumr.levels[i].clusters, j);

            int matches;
            struct bloom * filter = c->agg_filter;
            if (!filter) continue;

            log_print(g_log, "cluster = %d, searching aggregate filter", c->id);
            if ((matches = search_bloom(filter, name)) > longest_match) {
                lm_level = i;
                lm_clusterId = c->id;
                longest_match = matches;
                goto END;
            }
        }
    }

    END:

    if (longest_match > 0) {
        *level = lm_level;
        *clusterId = lm_clusterId;
    } else {
        /* we didn't find the content name in any of the bloom filters we have */
        if (amClusterHead(g_ccnumr.num_levels)) {
            /* I am the cluster head, I forward the message up one level */
            *level = g_ccnumr.num_levels - 1;
            *clusterId = clus_get_clusterHead(g_ccnumr.num_levels - 1);
        } else {
            /* I forward to my cluster head */
            *level = g_ccnumr.num_levels;
            *clusterId = clus_get_clusterId(g_ccnumr.num_levels);
        }
    }

    return 0;
}
