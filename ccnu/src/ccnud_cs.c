#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "ccnud_cs.h"
#include "hash.h"
#include "ccnud_constants.h"
#include "log.h"
#include "linked_list.h"

struct CS {
    struct hashtable * table;

    int summary_size;
};

struct CS_segment {
    struct content_obj * index_chunk;
    struct content_obj ** chunks;
    int num_chunks;
};

struct CS _cs;

static unsigned int random_evict(unsigned int * indices, unsigned int len);
static unsigned int oldest_evict(unsigned int * candidates, unsigned int len);

extern struct log * g_log;

static void segment_destroy(struct CS_segment * seg)
{
    content_obj_destroy(seg->index_chunk);
    int i;
    for (i = 0; i < seg->num_chunks; i++) {
        content_obj_destroy(seg->chunks[i]);
    }
    free(seg->chunks);
    free(seg);
}

int CS_init(evict_policy_t ep, double p)
{
    evict_t evict_fun;

    switch (ep) {
        case RANDOM:
            evict_fun = random_evict;
            break;
        case OLDEST:
            evict_fun = oldest_evict;
            break;
        case LRU:
            log_print(g_log, "LRU not implemented yet, defaulting to RANDOM policy");
            /* not implemented yet */
        default:
            evict_fun = random_evict;
            break;
    }

    _cs.table = hash_create(CS_SIZE, evict_fun, (delete_t)segment_destroy, BLOOM_ARGS);

    if (!_cs.table) return -1;

    /*     -n * ln(p)
     * m = ----------
     *      (ln(2))^2
     */
    _cs.summary_size = ceil(-1.0 * (CS_SIZE * log(p))) / (pow(log(2.0),2.0));
    int rem = _cs.summary_size % 8;
    /* round to nearest 8 since this is the number of bits for bloom filter */
    _cs.summary_size += (8 - rem);
    _cs.summary_size = (_cs.summary_size >> 3);
    log_print(g_log, "calculated cs summary Bloom filter size = %d", _cs.summary_size);

    return 0;
}

int CS_put(struct content_obj * content)
{
    struct CS_segment * segment = malloc(sizeof(struct CS_segment));
    /* this is a content matching a prefix */
    char * key = malloc(strlen(content->name->full_name));
    strcpy(key, content->name->full_name);
    segment->index_chunk = content;
    segment->chunks = NULL;
    segment->num_chunks = 0;

    hash_put(_cs.table, key, (void * ) segment);

    return 0;
}

int CS_putSegment(struct content_obj * prefix_obj, struct linked_list * content_chunks)
{
    if (!prefix_obj || !content_chunks) return -1;

    struct CS_segment * segment = malloc(sizeof(struct CS_segment));
    /* this is a content matching a prefix */
    char * key = malloc(strlen(prefix_obj->name->full_name));
    strcpy(key, prefix_obj->name->full_name);
    segment->index_chunk = prefix_obj;
    segment->num_chunks = content_chunks->len;
    segment->chunks = malloc(sizeof(struct content_obj * ) * segment->num_chunks);

    int i = 0;
    while (content_chunks->len) {
        segment->chunks[i++] = linked_list_remove(content_chunks, 0);
    }

    hash_put(_cs.table, key, (void * ) segment);
    return 0;
}

static int is_num(char * component)
{
    while (*component) {
        if (!isdigit(*component++)) {
            return 0;
        }
    }
    return 1;
}

struct content_obj * CS_get(struct content_name * name)
{
    if (!name) return NULL;

    char * last_component = content_name_getComponent(name, name->num_components - 1);
    struct CS_segment * segment = NULL;

    if (!is_num(last_component)) {
        segment = (struct CS_segment * ) hash_get(_cs.table, name->full_name);
        if (!segment) return NULL;
        return segment->index_chunk;
    } else {
        int chunk = atoi(last_component);
        char prefix[MAX_NAME_LENGTH];
        strncpy(prefix, name->full_name, name->len - 1 - strlen(last_component));
        prefix[name->len - strlen(last_component)] = '\0';
        segment = (struct CS_segment * ) hash_get(_cs.table, prefix);
        if (segment->num_chunks <= chunk) return NULL;
        return segment->chunks[chunk];
    }
}

struct content_obj * CS_getSegment(struct content_name * prefix)
{
    if (!prefix) return NULL;

    struct CS_segment * segment;
    segment = (struct CS_segment * ) hash_get(_cs.table, prefix->full_name);

    if (!segment || !segment->num_chunks) {
        return NULL;
    }

    struct content_obj * all = malloc(sizeof(struct content_obj));
    all->name = prefix;
    all->publisher = segment->index_chunk->publisher;
    all->timestamp = segment->index_chunk->timestamp;
    all->data = malloc(CCNU_MAX_PACKET_SIZE * segment->num_chunks);

    int i;
    int size = 0;
    for (i = 0; i < segment->num_chunks; i++) {
        struct content_obj * chunk = segment->chunks[i];
        memcpy(all->data + size, chunk->data, chunk->size);
        size += chunk->size;
    }
    all->size = size;
    return all;
}

int CS_summary(struct bloom ** filter_ptr)
{
    if (!filter_ptr) return -1;

    *filter_ptr = bloom_create(_cs.summary_size, BLOOM_ARGS);
    int i;
    for (i = 0; i < _cs.table->size; i++) {
        struct hash_entry * entry = _cs.table->entries[i];
        if (entry && entry->valid) {
            char * name = entry->key;
            bloom_add(*filter_ptr, name);
        }
    }

    return 0;
}

/* randomly chooses among the candidates to evict */
unsigned int random_evict(unsigned int *candidates, unsigned int len)
{
    int evict = candidates[rand() % len];
    log_print(g_log, "CS: evicting %d (criteria: random)", evict);
    return evict;
}

/* chooses the content with the oldest timestamp */
unsigned int oldest_evict(unsigned int * candidates, unsigned int len)
{
    int i, age, oldest_age, oldest_i = 0;
    struct CS_segment * segment;

    segment = hash_getAtIndex(_cs.table, candidates[0])->data;
    oldest_age = segment->index_chunk->timestamp;

    for (i = 1; i < len; i++) {
        segment = hash_getAtIndex(_cs.table, candidates[i])->data;
        age = segment->index_chunk->timestamp;

        if (age > oldest_age) {
            oldest_age = age;
            oldest_i = i;
        }
    }

    log_print(g_log, "CS: evicting %d (criteria: oldest)", candidates[oldest_i]);
    return candidates[oldest_i];
}
