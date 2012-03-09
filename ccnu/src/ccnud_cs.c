#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ccnud_cs.h"
#include "hash.h"
#include "ccnud_constants.h"
#include "log.h"

struct CS {
    struct hashtable * table;

    int summary_size;
};

struct CS _cs;

static unsigned int random_evict(unsigned int * indices, unsigned int len);
static unsigned int oldest_evict(unsigned int * candidates, unsigned int len);

extern struct log * g_log;

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

    _cs.table = hash_create(CS_SIZE, evict_fun, (delete_t)content_obj_destroy, BLOOM_ARGS);

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
    char * key = malloc(strlen(content->name->full_name));
    strcpy(key, content->name->full_name);
    hash_put(_cs.table, key, (void * ) content);
    return 0;
}

struct content_obj * CS_get(struct content_name * name)
{
    return (struct content_obj * ) hash_get(_cs.table, name->full_name);
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
    struct content_obj * content;

    content = ((struct content_obj *) hash_getAtIndex(_cs.table, candidates[0]));
    oldest_age = content->timestamp;

    for (i = 1; i < len; i++) {
        content = ((struct content_obj *) hash_getAtIndex(_cs.table, candidates[i]));
        age = content->timestamp;

        if (age > oldest_age) {
            oldest_age = age;
            oldest_i = i;
        }
    }

    log_print(g_log, "CS: evicting %d (criteria: oldest)", candidates[oldest_i]);
    return candidates[oldest_i];
}
