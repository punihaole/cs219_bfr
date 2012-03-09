/*
 *
 * I implement cuckoo hashing.
 *
 */
#include "hash.h"
#include "hashtable.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

struct hashtable * hash_create(int size, evict_t evict_fun, delete_t delete_fun, int nhashes, ...)
{
        struct hashtable * table = (struct hashtable * ) malloc(sizeof(struct hashtable));
        if (!table) return NULL;

        va_list l;

        table->entries = (struct hash_entry ** ) malloc(sizeof(struct hash_entry * ) * size);
        table->size = size;

        table->hash_funs = (hashfunc_t * ) malloc(sizeof(hashfunc_t) * nhashes);
        table->num_hash_funs = nhashes;
        int i;
        va_start(l, nhashes);
        for (i = 0; i < nhashes; i++) {
                table->hash_funs[i] = va_arg(l, hashfunc_t);
        }
        va_end(l);

        table->evict_fun = evict_fun;
        table->delete_fun = delete_fun;

        for (i = 0; i < size; i++) {
                table->entries[i] = (struct hash_entry * ) malloc(sizeof(struct hash_entry));
                table->entries[i]->key = NULL;
                table->entries[i]->data = NULL;
                table->entries[i]->valid = 0;
        }

        return table;
}

void delete_entry(struct hashtable * table, struct hash_entry * entry)
{
        if (entry->key) free(entry->key);
        if (entry->data) (*table->delete_fun)(entry->data);
}

void hash_delete(struct hashtable * table)
{
        int i;
        for (i = 0; i < table->size; i++) {
                delete_entry(table, table->entries[i]);
                free(table->entries[i]);
        }
        free(table->entries);
        free(table->hash_funs);
        free(table);
}

void hash_put(struct hashtable * table, char * name, void * data)
{
    if (!table || !name) return;

    int i;
    unsigned int * h = malloc(sizeof(unsigned int) * table->num_hash_funs);
	unsigned int hash;
    for (i = 0; i < table->num_hash_funs; i++) {
        h[i] = table->hash_funs[i](name) % table->size;
		if (table->entries[h[i]]->valid) {
			/* occupied */
			if (strcmp(table->entries[h[i]]->key, name) == 0) {
				/* keys are identical, just overwrite */
				(*table->delete_fun)(table->entries[h[i]]->data);
				free(table->entries[h[i]]->key);
				table->entries[h[i]]->valid = 0;
				break;
			}
		} else {
			/* we can use it */
			break;
		}
    }

    hash = h[i];
    // If h is the index of an occupied entry that means we exhausted all
    // possibilities and they were all taken. We must therefore must make
    // room.
    if (table->entries[hash]->valid) {
		hash = (*table->evict_fun)(h, table->num_hash_funs);
		(*table->delete_fun)(table->entries[hash]->data);
		free(table->entries[hash]->key);
	}
	free(h);

    table->entries[hash]->key = name;
    table->entries[hash]->data = data;
    table->entries[hash]->valid = 1;
}

void * hash_get(struct hashtable * table, const char * name)
{
        if (!table || !name) return NULL;

        int found = 0;
        struct hash_entry * temp;
        int i;
        for (i = 0; i < table->num_hash_funs; i++) {
                temp = table->entries[table->hash_funs[i](name) % table->size];
                if ((temp->key) && (!strcmp(temp->key, name))) {
                        found = 1;
                        break;
                }
        }

        if (!found) return NULL;

        return temp->data;
}

struct hash_entry * hash_getAtIndex(struct hashtable * table, unsigned int i)
{
        if (!table || (i > table->size)) return NULL;
        return table->entries[i];

}

int hash_remove(struct hashtable * table, const char * name)
{
        if (!table || !name) return 1;

        int found = 0;
        int i;
        unsigned int hash;
        for (i = 0; i < table->num_hash_funs; i++) {
                hash = table->hash_funs[i](name) % table->size;
                if (table->entries[hash]->key && strcmp(table->entries[hash]->key, name) == 0) {
                        found = 1;
                        break;
                }
        }

        if (!found) return 1;
        delete_entry(table, table->entries[hash]);
        table->entries[hash]->key = NULL;
        table->entries[hash]->data = NULL;
        table->entries[hash]->valid = 0;

        return 0;
}

#ifdef HASH_TAB_DEBUG

#include <stdio.h>
void hash_print(struct hashtable * table)
{
        int i;
        printf("-------------------------\n");
        for (i = 0; i < table->size; i++) {
                if (table->entries[i]->key)
                        printf("%i -> (%s)\n", i, table->entries[i]->key);
                else    printf("%i -> ()\n", i);
        }
        printf("-------------------------\n");
}
#endif
