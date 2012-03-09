/*
 * bloom_filter.h
 *
 */

#ifndef BLOOM_FILTER_H_INCLUDED
#define BLOOM_FILTER_H_INCLUDED

#include "bitmap.h"
#include "hash.h"

#define BLOOM_DEBUG

struct bloom {
        int size;
        struct bitmap * vector;
        hashfunc_t * hash_funs;
        int num_hash_funs;
};

/**
 * bloom_create
 *
 * Creates a bloom filter with variable number of hash functions and a given
 * bit array size.
 *
 * Optimal number of hash functions is ln(2) * (m/n)
 **/
struct bloom * bloom_create(int size, int nhashes, ...);

struct bloom * bloom_createFromVector(int size, unsigned int * vector, int nhashes, ...);

/**
 * bloom_destroy
 *
 * Deletes a bloom filter.
 **/
void bloom_destroy(struct bloom * filter);

/**
 * bloom_add
 *
 * Adds a element to our set.
 * Returns 1 if error, 0 on success.
 **/
int bloom_add(struct bloom * filter, const char * str);

/**
 * bloom_check
 *
 * Checks if the given element is in the set.
 * Returns 1 if the given string is in the bloom filter.
 **/
int bloom_check(struct bloom * filter, const char * str);

/**
 * bloom_or
 *
 * Does a bitwise OR of filter1 and filter2 and stores the result in result.
 * result should be an already initialized bloom filter of the correct size.
 * Returns 0 on success, 1 otherwise (no side-effect).
 **/
int bloom_or(struct bloom * filter1, struct bloom * filter2, struct bloom * result);

#ifdef BLOOM_DEBUG
void bloom_print(struct bloom * filter);
#endif

#endif // BLOOM_FILTER_H_INCLUDED
