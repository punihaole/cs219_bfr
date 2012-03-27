#ifndef CCNUD_PIT_H_INCLUDED
#define CCNUD_PIT_H_INCLUDED

#include <pthread.h>

#include "ccnud_constants.h"
#include "content.h"
#include "content_name.h"
#include "linked_list.h"

/*
 * Note that the registered field is like the faces notion of CCNx.
 * We only have 2 faces, network and applicaiton. Registered is set to 1
 * for application faces.
 */
typedef int PENTRY;

/* possible values for a PENTRY */
#define PIT_INVALID -1 /* entry not in PIT */
#define PIT_EXPIRED -2 /* entry is expired */
#define PIT_BUSY    -3 /* entry is currently being serviced */
#define PIT_FULL    -4 /* PIT is full and nothing could be evicted */
#define PIT_ARG_ERR -5 /* provided arguments were invalid */

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int rcv_window;
    int * max_window;
    struct linked_list * rcv_chunks;
    struct content_name * base;
} _segment_q_t;

struct pit;

extern struct pit g_pit;

int PIT_init();

/* used to get a handle into the PIT for an interest we express */
PENTRY PIT_get_handle(struct content_name * name);

/* used to add a PIT for interests we overheard and fwded */
int PIT_add_entry(struct content_name * name);

/* returns the pit entry handle with that exact content name */
PENTRY PIT_search(struct content_name * name);

/* returns the pit entry with the longest prefix match to that content name.
 * the _pit_entry will be returned in the locked state. This prevents the
 * pit GC from stomping on us.
 */
PENTRY PIT_longest_match(struct content_name * name);

/* returns 1 if the entry is expired */
int PIT_is_expired(PENTRY _pe);

struct timespec * PIT_expiration(PENTRY _pe);

/* PIT entry age in ms */
long PIT_age(PENTRY _pe);

/* removes an entry from the pit */
void PIT_release(PENTRY _pe);

/* closes the handle without invalidating the PIT table */
void PIT_close(PENTRY _pe);

void PIT_refresh(PENTRY _pe);

/* *obj will point to the content_obj pointer */
int PIT_point_data(PENTRY _pe, struct content_obj *** obj);

struct content_obj * PIT_get_data(PENTRY _pe);

/* does not overwrite data if already present */
int PIT_hand_data(PENTRY _pe, struct content_obj * obj);

void PIT_signal(PENTRY _pe);
int PIT_wait(PENTRY _pe, struct timespec * ts);

void PIT_lock(PENTRY _pe);
void PIT_unlock(PENTRY _pe);

/* returns 1 if the PIT is registered (i.e. expressed by a local application */
int PIT_is_registered(PENTRY _pe);

/* Debugging */
void PIT_print();

#endif // CCNUD_PIT_H_INCLUDED
