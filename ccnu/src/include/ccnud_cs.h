#ifndef CCNUD_CS_H_INCLUDED
#define CCNUD_CS_H_INCLUDED

#include "bloom_filter.h"
#include "ccnud_constants.h"
#include "content.h"
#include "hashtable.h"
#include "linked_list.h"

typedef enum {
    RANDOM,
    LRU,
    OLDEST
} evict_policy_t;

/* specify an eviction policy and the false positive probability, p */
int CS_init(evict_policy_t ep, double p);

int CS_putSegment(struct content_obj * prefix_obj, struct linked_list * content_chunks);

int CS_put(struct content_obj * content);

/**
 * @deprecated CS_getSegment
 **/
//struct content_obj * CS_getSegment(struct content_name * prefix);

struct content_obj * CS_get(struct content_name * name);

int CS_summary(struct bloom ** filter);

#endif // CCNUD_CS_H_INCLUDED
