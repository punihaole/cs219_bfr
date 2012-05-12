#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>

#include "ccnfd_cs.h"
#include "hash.h"
#include "ccnfd_constants.h"
#include "log.h"
#include "linked_list.h"
#include "bitmap.h"

struct CS {
    struct hashtable * table;
    int summary_size;
    pthread_mutex_t lock;
};

struct CS_segment {
    struct content_obj * index_chunk;
    struct content_obj ** chunks;
    int num_chunks;
    struct bitmap * valid;
    pthread_mutex_t lock;
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

    pthread_mutex_destroy(&seg->lock);
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
    pthread_mutex_init(&_cs.lock, NULL);

    if (!_cs.table) return -1;

    /* -n * ln(p)
* m = ----------
* (ln(2))^2
*/
    _cs.summary_size = ceil(-1.0 * (CS_SIZE * log(p))) / (pow(log(2.0),2.0));
    int rem = _cs.summary_size % 8;
    /* round to nearest 8 since this is the number of bits for bloom filter */
    _cs.summary_size += (8 - rem);
    log_print(g_log, "calculated cs summary Bloom filter size = %d bits", _cs.summary_size);

    return 0;
}

static inline struct content_obj * content_copy(struct content_obj * orig)
{
    if (!orig) {
        return NULL;
    }

    struct content_obj * copy = malloc(sizeof(struct content_obj));
    log_assert(g_log, copy != NULL, "CS: failed to allocate content");
    copy->publisher = orig->publisher;
    copy->name = content_name_create(orig->name->full_name);
    copy->timestamp = orig->timestamp;
    copy->size = orig->size;
    copy->data = malloc(sizeof(uint8_t) * copy->size);
    memcpy(copy->data, orig->data, copy->size);

    return copy;
}

int CS_put(struct content_obj * content)
{
    content = content_copy(content);
    log_assert(g_log, content != NULL, "CS: failed to allocate content");

    pthread_mutex_lock(&_cs.lock);
    struct CS_segment * segment = (struct CS_segment * )
        hash_get(_cs.table, content_prefix(content->name));
    pthread_mutex_unlock(&_cs.lock);

    int rv = 0;
    if (content_is_segmented(content->name)) {

        if (segment) {

            pthread_mutex_lock(&segment->lock);

            int seq_no = content_seq_no(content->name);
            if (seq_no >= segment->num_chunks) {
                segment->chunks = realloc(segment->chunks, sizeof(struct content_obj * ) * (seq_no + 1));
                segment->num_chunks = seq_no + 1;
                segment->chunks[seq_no] = content;
                struct bitmap * larger = bit_create(segment->num_chunks);
                memcpy(larger->map, segment->valid->map, segment->valid->num_words);
                bit_destroy(segment->valid);
                segment->valid = larger;
                bit_set(larger, seq_no);
            } else {
                if (bit_test(segment->valid, seq_no)) {
                    content_obj_destroy(segment->chunks[seq_no]);
                }
                segment->chunks[seq_no] = content;
                bit_set(segment->valid, seq_no);
            }

            pthread_mutex_unlock(&segment->lock);

        } else {
            rv = -1;
        }

    } else {

        if (!segment) {

            segment = malloc(sizeof(struct CS_segment));
            /* this is a content matching a prefix */
            char * key = malloc(strlen(content->name->full_name));
            strcpy(key, content->name->full_name);
            segment->index_chunk = content;
            segment->chunks = NULL;
            segment->num_chunks = -1;
            segment->valid = bit_create(0);
            pthread_mutex_init(&segment->lock, NULL);

            pthread_mutex_lock(&_cs.lock);
            hash_put(_cs.table, key, (void * ) segment);
            pthread_mutex_unlock(&_cs.lock);

        } else {

            pthread_mutex_lock(&segment->lock);

            if (segment->index_chunk) {
                content_obj_destroy(segment->index_chunk);
            }
            segment->index_chunk = content;

            pthread_mutex_unlock(&segment->lock);

        }
    }

    return rv;
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
    segment->valid = bit_create(segment->num_chunks);
    pthread_mutex_init(&segment->lock, NULL);

    int i = 0;
    while (content_chunks->len) {
        bit_set(segment->valid, i);
        segment->chunks[i++] = linked_list_remove(content_chunks, 0);
    }

    pthread_mutex_lock(&_cs.lock);
    hash_put(_cs.table, key, (void * ) segment);
    pthread_mutex_unlock(&_cs.lock);

    return 0;
}

struct content_obj * CS_get(struct content_name * name)
{
    if (!name) return NULL;

    struct CS_segment * segment = NULL;
    struct content_obj * copy = NULL;

    if (!content_is_segmented(name)) {

        pthread_mutex_lock(&_cs.lock);
        segment = (struct CS_segment * ) hash_get(_cs.table, name->full_name);
        pthread_mutex_unlock(&_cs.lock);

        if (segment) {

            pthread_mutex_lock(&segment->lock);
            copy = content_copy(segment->index_chunk);
            pthread_mutex_unlock(&segment->lock);

        }

        return copy;

    } else {

        int chunk = content_seq_no(name);
        char * prefix = content_prefix(name);

        pthread_mutex_lock(&_cs.lock);
        segment = (struct CS_segment * ) hash_get(_cs.table, prefix);
        pthread_mutex_unlock(&_cs.lock);

        if ((segment) && (chunk < segment->num_chunks)) {

            pthread_mutex_lock(&segment->lock);
            copy = content_copy(segment->chunks[chunk]);
            pthread_mutex_unlock(&segment->lock);

        }

        free(prefix);

        return copy;

    }
}

/*
struct content_obj * CS_getSegment(struct content_name * prefix)
{
    if (!prefix) return NULL;

    pthread_mutex_lock(&_cs.lock);
    struct CS_segment * segment =
        (struct CS_segment * ) hash_get(_cs.table, prefix->full_name);
    pthread_mutex_unlock(&_cs.lock);

    struct content_obj * all = NULL;

    if (segment) {

        pthread_mutex_lock(&segment->lock);

        if (segment->num_chunks >= 0) {

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
        }

        pthread_mutex_unlock(&segment->lock);

    }

    return all;
}
*/

int CS_summary(struct bloom ** filter_ptr)
{
    if (!filter_ptr) return -1;

    *filter_ptr = bloom_create(_cs.summary_size, BLOOM_ARGS);
    int i;

    pthread_mutex_lock(&_cs.lock);

    for (i = 0; i < _cs.table->size; i++) {
        struct hash_entry * entry = _cs.table->entries[i];
        if (entry && entry->valid) {
            char * name = entry->key;
            bloom_add(*filter_ptr, name);
        }
    }

    pthread_mutex_unlock(&_cs.lock);

    return 0;
}

/* randomly chooses among the candidates to evict */
unsigned int random_evict(unsigned int *candidates, unsigned int len)
{
    int evict = candidates[rand() % len];
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

    return candidates[oldest_i];
}
