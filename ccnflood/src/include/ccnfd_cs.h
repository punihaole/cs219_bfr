#ifndef CCNUD_CS_H_INCLUDED
#define CCNUD_CS_H_INCLUDED

#include "bloom_filter.h"
#include "ccnfd_constants.h"
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

/* Places the content objects in the content store. Does not copy the segments */
int CS_putSegment(struct content_obj * prefix_obj, struct linked_list * content_chunks);

/* Copies and stores the content. Caller should free the content obj */
int CS_put(struct content_obj * content);

/* Returns a copy of the segment collapsed into one content object */
//struct content_obj * CS_getSegment(struct content_name * prefix);

/* Returns a copy of the given content */
struct content_obj * CS_get(struct content_name * name);

int CS_summary(struct bloom ** filter);

#endif // CCNUD_CS_H_INCLUDED
