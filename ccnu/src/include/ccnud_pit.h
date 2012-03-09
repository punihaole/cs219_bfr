#ifndef CCNUD_PIT_H_INCLUDED
#define CCNUD_PIT_H_INCLUDED

#include <pthread.h>

#include "content.h"
#include "content_name.h"
#include "linked_list.h"

/* these handles are dynamically created. You should free them yourself or
 * use pit_release to free the handle and clear the pit entry.
 * Note that the registered field is like the faces notion of CCNx.
 * We only have 2 faces, network and applicaiton. Registered is set to 1
 * for application faces.
 */
#define PENTRY _pit_entry_s *
typedef struct {
    pthread_mutex_t * mutex;
    pthread_cond_t * cond;
    struct content_obj ** obj;
    int index;
    int registered;
} _pit_entry_s; /* no touching */

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

/* removes an entry from the pit */
void PIT_release(PENTRY _pe);

void PIT_refresh(PENTRY _pe);

#endif // CCNUD_PIT_H_INCLUDED
