#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "bloom_filter.h"
#include "bitmap.h"

static struct bloom * bloom_vcreate(int size, int nhashes, va_list ap)
{
	struct bloom * filter = (struct bloom * ) malloc(sizeof(struct bloom));
	if (!filter) return NULL;

	filter->size = size;
	filter->vector = bit_create(size);

	if (!filter->vector) {
		free(filter);
		return NULL;
	}

	filter->hash_funs = (hashfunc_t * ) malloc(sizeof(hashfunc_t) * nhashes);
	if (!filter->hash_funs) {
		free(filter->vector);
		free(filter);
	}

	int i;
	for (i = 0; i < nhashes; i++) {
		filter->hash_funs[i] = va_arg(ap, hashfunc_t);
	}
	filter->num_hash_funs = nhashes;

	return filter;
}

struct bloom * bloom_create(int size, int nhashes, ...)
{
	va_list argp;
	va_start(argp, nhashes);
	struct bloom * filter = bloom_vcreate(size, nhashes, argp);
	va_end(argp);
	return filter;
}

struct bloom * bloom_createFromVector(int size, unsigned int * vector, int nhashes, ...)
{
    va_list argp;
    va_start(argp, nhashes);
    struct bloom * filter = bloom_vcreate(size, nhashes, argp);
	va_end(argp);
	if (!filter) return NULL;

    memcpy(filter->vector->map, vector, size/8);
    return filter;
}

void bloom_destroy(struct bloom * filter)
{
        if (!filter)
                return;
        bit_destroy(filter->vector);
        free(filter->hash_funs);
        free(filter);
		filter = NULL;
}

int bloom_add(struct bloom * filter, const char * str)
{
        if (!filter || !str)
                return 1;
        int i, index;
        for (i = 0; i < filter->num_hash_funs; i++) {
                index = filter->hash_funs[i](str) % filter->size;
                bit_set(filter->vector, index);
        }

        return 0;
}

int bloom_check(struct bloom * filter, const char * str)
{
        if (!filter || !str)
                return 0;
        int i;
        for (i = 0; i < filter->num_hash_funs; i++) {
                if (bit_test(filter->vector, filter->hash_funs[i](str) % filter->size) != 1)
                        return 0; /* if any of the bits are not set than we return false */
        }

        return 1;
}

int bloom_or(struct bloom * filter1, struct bloom * filter2, struct bloom * result)
{
	if (!filter1 || !filter2 || !result)
		return 1;
	if (filter1->size != filter2->size)
		return 1;

	unsigned int * vector1 = filter1->vector->map;
	unsigned int * vector2 = filter2->vector->map;
	unsigned int * vectord = result->vector->map;

	unsigned int vecSize = filter1->vector->num_bits;
	if (vecSize != filter2->vector->num_bits || vecSize != result->vector->num_bits)
		return 1;

    unsigned int iter = filter1->vector->num_words;
    int i;
	for (i = 0; i < iter; i++) {
		vectord[i] = vector1[i] | vector2[i];
	}

	return 0;
}

#ifdef BLOOM_DEBUG
#include <stdio.h>

void bloom_print(struct bloom * filter)
{
	bit_print(filter->vector);
}
#endif
