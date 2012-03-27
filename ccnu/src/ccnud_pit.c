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

static bool_t check_handle(PENTRY _pe)
{
    if (_pe < 0 || _pe >= PIT_SIZE)
        return FALSE;
    return TRUE;
}

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
        g_pit.pit_table[i].available = FALSE;
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
    if (!name) return PIT_ARG_ERR;

    pthread_mutex_lock(&g_pit.pit_lock);
    /* bit find also set the bit it returns */
    int index = bit_find(g_pit.pit_table_valid);

    if (index < 0) {
        /* we need to try to drop the oldest pit entry, or clean out expired */
        index = evict();
        if (index == -1)
            return PIT_FULL;

        /* we can evict this pit entry */
        if (g_pit.pit_table[index].obj) {
            g_pit.pit_table[index].obj = NULL;
        }

        if (g_pit.pit_table[index].name) {
            content_name_delete(g_pit.pit_table[index].name);
            g_pit.pit_table[index].name = NULL;
        }
    }

    PENTRY pe = index;
    ts_fromnow(&g_pit.pit_table[index].created);
    g_pit.pit_table[index].expires = g_pit.pit_table[index].created;
    ts_addms(&g_pit.pit_table[index].expires, g_pit.pit_lifetime_ms);
    g_pit.pit_table[index].name = content_name_create(name->full_name);
    g_pit.pit_table[index].registered = 1;
    g_pit.pit_table[index].available = TRUE;
    pthread_mutex_lock(&g_pit.pit_table[index].mutex);
    pthread_mutex_unlock(&g_pit.pit_lock);

    return pe;
}

int PIT_add_entry(struct content_name * name)
{
    if (!name) return -1;

    pthread_mutex_lock(&g_pit.pit_lock);
    /* bit find also set the bit it returns */
    int index = bit_find(g_pit.pit_table_valid);

    if (index < 0) {
        /* we need to try to drop the oldest pit entry, or clean out expired */
        index = evict();
        if (index == -1)
            return -1;

        /* we can evict this pit entry */
        if (g_pit.pit_table[index].obj) {
            g_pit.pit_table[index].obj = NULL;
        }

        g_pit.pit_table[index].registered = 0;
        g_pit.pit_table[index].available = TRUE;

        if (g_pit.pit_table[index].name) {
            content_name_delete(g_pit.pit_table[index].name);
            g_pit.pit_table[index].name = NULL;
        }
    }

    ts_fromnow(&g_pit.pit_table[index].expires);
    ts_addms(&g_pit.pit_table[index].expires, g_pit.pit_lifetime_ms);
    g_pit.pit_table[index].name = content_name_create(name->full_name);
    g_pit.pit_table[index].registered = 0;
    g_pit.pit_table[index].available = TRUE;

    pthread_mutex_unlock(&g_pit.pit_lock);

    return 0;
}

PENTRY PIT_search(struct content_name * name)
{
    int index = -1;
    log_print(g_log, "PIT_search trying to lock table");
    pthread_mutex_lock(&g_pit.pit_lock);
    log_print(g_log, "PIT_search locked");
    int i;
    for (i = 0; i < PIT_SIZE; i++) {
        if (!bit_test(g_pit.pit_table_valid, i)) {
            continue;
        }

        if (strcmp(g_pit.pit_table[i].name->full_name, name->full_name) == 0) {
            index = i;
            log_print(g_log, "PIT search found %s = %d", name->full_name, index);
            break;
        }
    }

    if (index == -1) {
        pthread_mutex_unlock(&g_pit.pit_lock);
        return PIT_INVALID;
    }

    if (g_pit.pit_table[index].available == FALSE) {
        log_print(g_log, "PIT search found %s = %d, available = NO", name->full_name, index);
        pthread_mutex_unlock(&g_pit.pit_lock);
        return PIT_BUSY;
    }

    /*struct timespec now;
    ts_fromnow(&now);
    if (ts_compare(&g_pit.pit_table[index].expires, &now) > 0) {
        pthread_mutex_unlock(&g_pit.pit_lock);
        return PIT_EXPIRED;
    }*/

    PENTRY pe = index;
    g_pit.pit_table[index].available = FALSE;
    pthread_mutex_unlock(&g_pit.pit_lock);
    pthread_mutex_lock(&g_pit.pit_table[index].mutex);

    return pe;
}

PENTRY PIT_longest_match(struct content_name * name)
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

    if (index == -1) {
        pthread_mutex_unlock(&g_pit.pit_lock);
        return PIT_INVALID;
    }

    if (!g_pit.pit_table[index].available) {
        pthread_mutex_unlock(&g_pit.pit_lock);
        return PIT_BUSY;
    }

    /*struct timespec now;
    ts_fromnow(&now);
    if (ts_compare(&g_pit.pit_table[index].expires, &now) > 0) {
        pthread_mutex_unlock(&g_pit.pit_lock);
        return PIT_EXPIRED;
    }*/

    PENTRY pe = index;
    g_pit.pit_table[index].available = FALSE;
    pthread_mutex_lock(&g_pit.pit_table[index].mutex);
    pthread_mutex_unlock(&g_pit.pit_lock);

    return pe;
}

void PIT_release(PENTRY _pe)
{
    if (check_handle(_pe)) {
        pthread_mutex_lock(&g_pit.pit_lock);
            pthread_mutex_unlock(&g_pit.pit_table[_pe].mutex);
            bit_clear(g_pit.pit_table_valid, _pe);
            content_name_delete(g_pit.pit_table[_pe].name);
            g_pit.pit_table[_pe].name = NULL;
            g_pit.pit_table[_pe].obj = NULL;
            g_pit.pit_table[_pe].registered = 0;
            g_pit.pit_table[_pe].available = TRUE;
        pthread_mutex_unlock(&g_pit.pit_lock);
    }
}

void PIT_refresh(PENTRY _pe)
{
    if (check_handle(_pe)) {
        ts_fromnow(&g_pit.pit_table[_pe].created);
        memcpy(&g_pit.pit_table[_pe].expires, &g_pit.pit_table[_pe].created, sizeof(struct timespec));
        ts_addms(&g_pit.pit_table[_pe].expires, g_pit.pit_lifetime_ms);
    }
}

void PIT_close(PENTRY _pe)
{
    if (check_handle(_pe)) {
        PIT_unlock(_pe);
        pthread_mutex_lock(&g_pit.pit_lock);
            g_pit.pit_table[_pe].available = TRUE;
        pthread_mutex_unlock(&g_pit.pit_lock);
    }
}

int PIT_is_expired(PENTRY _pe)
{
    if (check_handle(_pe)) {
        struct timespec now;
        ts_fromnow(&now);

        if (ts_compare(&g_pit.pit_table[_pe].expires, &now) > 0) {
            return 0;
        } else {
            return 1;
        }
    }

    return -1;
}

struct timespec * PIT_expiration(PENTRY _pe)
{
    struct timespec * ts = NULL;
    if (check_handle(_pe)) {
        ts = &g_pit.pit_table[_pe].expires;
    }

    return ts;
}

long PIT_age(PENTRY _pe)
{
    if (check_handle(_pe)) {
        struct timespec now;
        ts_fromnow(&now);

        long ms = ts_mselapsed(&g_pit.pit_table[_pe].created, &now);
        return ms;
    }

    return -1;
}

/* *obj will point to the content_obj pointer */
int PIT_point_data(PENTRY _pe, struct content_obj *** obj)
{
    if (check_handle(_pe)) {
        *obj = &g_pit.pit_table[_pe].obj;
        return 0;
    }

    return -1;
}

struct content_obj * PIT_get_data(PENTRY _pe)
{
    struct content_obj * data = NULL;
    if (check_handle(_pe)) {
        data = g_pit.pit_table[_pe].obj;
    }

    return data;
}

int PIT_hand_data(PENTRY _pe, struct content_obj * obj)
{
    if (check_handle(_pe)) {
        g_pit.pit_table[_pe].obj = obj;
        return 0;
    }
    return -1;
}

void PIT_signal(PENTRY _pe)
{
    if (check_handle(_pe)) {
        pthread_cond_signal(&g_pit.pit_table[_pe].cond);
    }
}

int PIT_wait(PENTRY _pe, struct timespec * ts)
{
    if (check_handle(_pe)) {
        return pthread_cond_timedwait(&g_pit.pit_table[_pe].cond, &g_pit.pit_table[_pe].mutex, ts);
    }
    return -1;
}

void PIT_lock(PENTRY _pe)
{
    if (check_handle(_pe)) {
        pthread_mutex_lock(&g_pit.pit_table[_pe].mutex);
    }
}

void PIT_unlock(PENTRY _pe)
{
    if (check_handle(_pe)) {
        pthread_mutex_unlock(&g_pit.pit_table[_pe].mutex);
    }
}

/* returns 1 if the PIT is registered (i.e. expressed by a local application */
int PIT_is_registered(PENTRY _pe)
{
    if (check_handle(_pe)) {
        return g_pit.pit_table[_pe].registered;
    }

    return -1;
}

void PIT_print()
{
    pthread_mutex_lock(&g_pit.pit_lock);
        int i;
        for (i = 0; i < PIT_SIZE; i++) {
            if (bit_test(g_pit.pit_table_valid, i) == 1) {
                log_print(g_log, "PIT[%d] = %s", i, g_pit.pit_table[i].name->full_name);
                log_print(g_log, "\tExpires: %d", g_pit.pit_table[i].expires.tv_sec);
            }
        }
    pthread_mutex_unlock(&g_pit.pit_lock);
}
