#ifndef HASHTABLE_H_INCLUDED
#define HASHTABLE_H_INCLUDED

#include "hash.h"

typedef unsigned int (*evict_t)(unsigned int *, unsigned int);
typedef void (*delete_t)(void *);

struct hash_entry {
        char * key;
        void * data;
        unsigned char valid;
};

struct hashtable {
        struct hash_entry ** entries;
        int size;
        hashfunc_t * hash_funs;
        int num_hash_funs;
        evict_t evict_fun;
        delete_t delete_fun;
};

/**
 * hash_create
 *
 * Creates a cuckoo hash table. This is a hash table that resolves collisions
 * via cuckoo hashing (you resort to another hash function(s)). If all attempts
 * collide than we call the specified evict function. This is useful because
 * many of our data structures are really just caches with fixed size. They may
 * have a useful eviction scheme such as LRU.
 * The data stored in the table is a void * so a deletion function must be
 * provided. It is the responsibility of the evict function to cleanup dangling
 * pointers.
 **/
struct hashtable * hash_create(int size, evict_t evict_fun, delete_t delete_fun, int nhashes, ...);

/**
 * hash_delete
 **/
void hash_delete(struct hashtable * table);

/**
 * hash_put
 *
 * Puts a piece of data with given content name into the hash table.
 * Resolves conflicts with cuckoo hashing and eviction policy defined elsewhere.
 **/
void hash_put(struct hashtable * table, char * name, void * data);

/**
 * hash_get
 *
 * Retrieves data (void *) with given content name.
 **/
void * hash_get(struct hashtable * table, const char * name);

/**
 * hash_getAtIndex
 *
 * Retrieves hash_entry at given index in backing array. This is needed by the
 * eviction function since it takes indices as arguments.
 **/
struct hash_entry * hash_getAtIndex(struct hashtable * table, unsigned int i);

/**
 * hash_remove
 *
 * Removes data from hash table.
 **/
int hash_remove(struct hashtable * table, const char * name);

#ifdef HASH_TAB_DEBUG
void hash_print(struct hashtable * table);
#endif

#endif // HASHTABLE_H_INCLUDED
