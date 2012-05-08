#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#include "ccnud_pit.h"
#include "ccnud_constants.h"
#include "bitmap.h"
#include "ts.h"
#include "log.h"

struct pit_entry {
    pthread_mutex_t mutex;
    pthread_cond_t cond; /* sleeps on this condition until we rcv the data */
    struct content_name * name;
    struct content_obj * obj;
    struct timespec expires;
    struct timespec created;
    int registered; /* set to 1 if a thread is waiting on this pit entry */
    bool_t available;
};

struct pit {
    pthread_mutex_t pit_lock;
    struct pit_entry pit_table[PIT_SIZE];
    struct bitmap * pit_table_valid; /* marks which entries are occiped */
    long pit_lifetime_ms;
};

struct pit g_pit;
extern struct log * g_log;

int PIT_init()
{
    g_pit.pit_table_valid = bit_create(PIT_SIZE);
    g_pit.pit_lifetime_ms = PIT_LIFETIME_MS;
    if (pthread_mutex_init(&g_pit.pit_lock, NULL) != 0) return -1;

    int i;
    for (i = 0; i < PIT_SIZE; i++) {
        if (pthread_mutex_init(&g_pit.pit_table[i].mutex, NULL) != 0) return -1;
        if (pthread_cond_init(&g_pit.pit_table[i].cond, NULL) != 0) return -1;
        g_pit.pit_table[i].name = NULL;
        g_pit.pit_table[i].obj = NULL;
        g_pit.pit_table[i].registered = 0;
        g_pit.pit_table[i].available = 0;
    }

    return 0;
}

/* we have locked down the pit. We don't touch any registered entries
 * must be run while the pit is LOCKED!!!
 */
static int evict()
{
    int rv;
    int i;
    int oldest = -1;
    struct timespec * oldest_age = NULL;

    for (i = 0; i < PIT_SIZE; i++) {
        rv = pthread_mutex_trylock(&g_pit.pit_table[i].mutex);

        if (rv == EBUSY) {
            /* we don't have the lock, pit entry is being serviced right now */
            continue;
        }

        /* we have the mutex */
        if (g_pit.pit_table[i].registered) {
            /* we don't evict registered pit entries */
            pthread_mutex_unlock(&g_pit.pit_table[i].mutex);
            continue;
        }

        /* candidate for evicition */
        if (!oldest_age) {
            oldest_age = &g_pit.pit_table[i].expires;
            oldest = i;
        } else {
            if (ts_compare(oldest_age, &g_pit.pit_table[i].expires)) {
                oldest_age = &g_pit.pit_table[i].expires;
                oldest = i;
            }
        }

        pthread_mutex_unlock(&g_pit.pit_table[i].mutex);
    }

    return oldest;
}

PENTRY PIT_get_handle(struct content_name * name)
{
    if (!name) return NULL;

    pthread_mutex_lock(&g_pit.pit_lock);
    /* bit find also set the bit it returns */
    int index = bit_find(g_pit.pit_table_valid);

    if (index < 0) {
        /* we need to try to drop the oldest pit entry, or clean out expired */
        index = evict();
        if (index == -1)
            return NULL;

        /* we can evict this pit entry */
        if (g_pit.pit_table[index].obj) {
            g_pit.pit_table[index].obj = NULL;
        }

        if (g_pit.pit_table[index].name) {
            content_name_delete(g_pit.pit_table[index].name);
            g_pit.pit_table[index].name = NULL;
        }
    }

    PENTRY pe = (PENTRY) malloc(sizeof(_pit_entry_s));
    pe->mutex = &g_pit.pit_table[index].mutex;
    pe->cond = &g_pit.pit_table[index].cond;
    pe->obj = &g_pit.pit_table[index].obj;

    ts_fromnow(&g_pit.pit_table[index].created);
    g_pit.pit_table[index].expires = g_pit.pit_table[index].created;
    ts_addms(&g_pit.pit_table[index].expires, g_pit.pit_lifetime_ms);
    pe->expires = &g_pit.pit_table[index].expires;

    g_pit.pit_table[index].name = content_name_create(name->full_name);
    pe->index = index;

    g_pit.pit_table[index].registered = 1;
    pe->registered = 1;

    g_pit.pit_table[index].available = 1;

    pthread_mutex_lock(&g_pit.pit_table[index].mutex);
    pthread_mutex_unlock(&g_pit.pit_lock);

    return pe;
}

int PIT_add_entry(struct content_name * name)
{
    if (!name) return -1;

    pthread_mutex_lock(&g_pit.pit_lock);
    // bit find also set the bit it returns
    int index = bit_find(g_pit.pit_table_valid);

    if (index < 0) {
        // we need to try to drop the oldest pit entry, or clean out expired
        index = evict();
        if (index == -1)
            return -1;

        // we can evict this pit entry
        if (g_pit.pit_table[index].obj) {
            g_pit.pit_table[index].obj = NULL;
        }

        if (g_pit.pit_table[index].name) {
            content_name_delete(g_pit.pit_table[index].name);
            g_pit.pit_table[index].name = NULL;
        }
    }
    ts_fromnow(&g_pit.pit_table[index].expires);
    ts_addms(&g_pit.pit_table[index].expires, g_pit.pit_lifetime_ms);
    g_pit.pit_table[index].name = content_name_create(name->full_name);
    g_pit.pit_table[index].registered = 0;
    g_pit.pit_table[index].available = 1;

    pthread_mutex_unlock(&g_pit.pit_lock);

    return 0;
}

PENTRY PIT_exact_match(struct content_name * name)
{
    int i;
    int match = 0;
    pthread_mutex_lock(&g_pit.pit_lock);
    for (i = 0; i < PIT_SIZE; i++) {
        if (!bit_test(g_pit.pit_table_valid, i)) {
            continue;
        }

        if (strcmp(g_pit.pit_table[i].name->full_name, name->full_name) == 0) {
            match = 1;
            break;
        }
    }

    struct timespec now;
    ts_fromnow(&now);

    int valid = 1;
    if (match) {
        pthread_mutex_lock(&g_pit.pit_table[i].mutex);
        pthread_mutex_unlock(&g_pit.pit_lock);

        if (ts_compare(&g_pit.pit_table[i].expires, &now) < 0) {
            valid = 1;
        } else if (g_pit.pit_table[i].available == 0) {
            valid = 1;
        }

        PENTRY pe = (PENTRY) malloc(sizeof(_pit_entry_s));
        pe->mutex = &g_pit.pit_table[i].mutex;
        pe->cond = &g_pit.pit_table[i].cond;
        pe->obj = &g_pit.pit_table[i].obj;
        pe->index = i;
        pe->registered = g_pit.pit_table[i].registered;
        pe->expires = &g_pit.pit_table[i].expires;
        g_pit.pit_table[i].available = 0;

        if (valid) {
            return pe;
        } else {
             pthread_mutex_unlock(&g_pit.pit_table[i].mutex);
             free(pe);
             return NULL;
        }
    } else {
        pthread_mutex_unlock(&g_pit.pit_lock);
        return NULL;
    }
}

/*PENTRY PIT_longest_match(struct content_name * name)
{
    int index = -1;
    int match_len = 0;
    int longest_match = 0;
    int i;

    pthread_mutex_lock(&g_pit.pit_lock);
    for (i = 0; i < PIT_SIZE; i++) {
        if (!bit_test(g_pit.pit_table_valid, i))
            continue;

        if ((match_len =
             content_name_prefixMatch(g_pit.pit_table[i].name, name)) > longest_match) {
            longest_match = match_len;
            index = i;
        }
        if (match_len == name->num_components)
            break;
    }

    struct timespec now;
    ts_fromnow(&now);

    if (index != -1 && ts_compare(&g_pit.pit_table[index].expires, &now)
        && g_pit.pit_table[index].available) {

        PENTRY pe = (PENTRY) malloc(sizeof(_pit_entry_s));
        pe->mutex = &g_pit.pit_table[index].mutex;
        pe->cond = &g_pit.pit_table[index].cond;
        pe->obj = &g_pit.pit_table[index].obj;
        pe->index = index;
        pe->registered = g_pit.pit_table[index].registered;
        pe->expires = &g_pit.pit_table[index].expires;
        g_pit.pit_table[index].available = 0;
        pthread_mutex_unlock(&g_pit.pit_lock);
        pthread_mutex_lock(&g_pit.pit_table[index].mutex);
        return pe;

    } else {

        pthread_mutex_unlock(&g_pit.pit_lock);
        return NULL;
    }
}*/

void PIT_release(PENTRY _pe)
{
    pthread_mutex_unlock(_pe->mutex);

    pthread_mutex_lock(&g_pit.pit_lock);
        bit_clear(g_pit.pit_table_valid, _pe->index);
        content_name_delete(g_pit.pit_table[_pe->index].name);
        g_pit.pit_table[_pe->index].name = NULL;
        g_pit.pit_table[_pe->index].obj = NULL;
        g_pit.pit_table[_pe->index].registered = 0;
        g_pit.pit_table[_pe->index].available = 0;
    pthread_mutex_unlock(&g_pit.pit_lock);

    free(_pe);
}

void PIT_refresh(PENTRY _pe)
{
    if (!_pe || _pe->index < 0) return;

    ts_fromnow(&g_pit.pit_table[_pe->index].created);
    memcpy(&g_pit.pit_table[_pe->index].expires, &g_pit.pit_table[_pe->index].created, sizeof(struct timespec));
    ts_addms(&g_pit.pit_table[_pe->index].expires, g_pit.pit_lifetime_ms);
}

void PIT_close(PENTRY _pe)
{
    pthread_mutex_unlock(_pe->mutex);
    pthread_mutex_lock(&g_pit.pit_lock);
        g_pit.pit_table[_pe->index].available = 1;
    pthread_mutex_unlock(&g_pit.pit_lock);
    free(_pe);
}

int PIT_is_expired(PENTRY _pe)
{
    struct timespec now;
    ts_fromnow(&now);

    if (ts_compare(&g_pit.pit_table[_pe->index].expires, &now) > 0) {
        return 0;
    } else {
        return 1;
    }
}

long PIT_age(PENTRY _pe)
{
    struct timespec now;
    ts_fromnow(&now);

    long ms = ts_mselapsed(&g_pit.pit_table[_pe->index].created, &now);
    return ms;
}

void PIT_print()
{
    pthread_mutex_lock(&g_pit.pit_lock);
        int i;
        for (i = 0; i < PIT_SIZE; i++) {
            if (bit_test(g_pit.pit_table_valid, i) == 1) {
                log_debug(g_log, "PIT[%d] = %s", i, g_pit.pit_table[i].name->full_name);
                log_debug(g_log, "\tExpires: %d", g_pit.pit_table[i].expires.tv_sec);
            }
        }
    pthread_mutex_unlock(&g_pit.pit_lock);
}
