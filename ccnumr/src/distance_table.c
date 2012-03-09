#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#include "bitmap.h"
#include "hash.h"

#include "ccnumrd_constants.h"

#include "distance_table.h"
#include "ts.h"

struct dist_tab_entry {
    char * name;
    int len;
    struct timespec expiration;
    int hops;
};

struct dist_table
{
    int size;
    struct bitmap * valid;
    pthread_mutex_t table_lock;
    struct dist_tab_entry * table;
};

struct dist_table g_dist_tab;

int dtab_init(int size)
{
    g_dist_tab.size = size;
    g_dist_tab.valid = bit_create(size);
    pthread_mutex_init(&g_dist_tab.table_lock, NULL);
    g_dist_tab.table = (struct dist_tab_entry * )
        malloc(sizeof(struct dist_tab_entry) * size);

    int i;
    for (i = 0; i < size; i++) {
        g_dist_tab.table[i].name = NULL;
    }

    return 0;
}

int dtab_getHops(char * name)
{
    int index = HASH(name) % g_dist_tab.size;
    int hops = -1;

    pthread_mutex_lock(&g_dist_tab.table_lock);
    if (bit_test(g_dist_tab.valid, index) == 1) {
        /* we need to overwrite it */
        struct timespec now;
        ts_fromnow(&now);
        if (ts_compare(&now, &g_dist_tab.table[index].expiration) < 0) {
            /* not expired */
            hops = g_dist_tab.table[index].hops;
        }
    }
    pthread_mutex_unlock(&g_dist_tab.table_lock);

    return hops;
}

int dtab_setHops(char * name, int hops)
{
    int index = HASH(name) % g_dist_tab.size;

    pthread_mutex_lock(&g_dist_tab.table_lock);
    if (bit_test(g_dist_tab.valid, index) == 1) {
        /* we need to overwrite it */
        free(g_dist_tab.table[index].name);
        g_dist_tab.table[index].name = malloc(strlen(name));
        strcpy(g_dist_tab.table[index].name, name);
    } else {
        g_dist_tab.table[index].name = malloc(strlen(name));
        strcpy(g_dist_tab.table[index].name, name);
        bit_set(g_dist_tab.valid, index);
    }
    pthread_mutex_unlock(&g_dist_tab.table_lock);

    return 0;
}
