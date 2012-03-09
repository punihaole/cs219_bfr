/*
 * hash.h
 *
 *  Created on: Sep 1, 2011
 *      Author: Thomas Punihaole
 *
 * A collection of hash functions for use in the hashtables and bloom filters.
 * The arguments must be null terminated strings.
 */


#ifndef HASH_H_INCLUDED
#define HASH_H_INCLUDED

typedef unsigned int (*hashfunc_t)(const char * );

unsigned int elfhash(const char * str);

unsigned int sdbmhash(const char * str);

unsigned int djbhash(const char * str);

unsigned int dekhash(const char * str);

unsigned int bphash(const char* str);

#endif // HASH_H_INCLUDED
