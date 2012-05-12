#include <stdlib.h>
#include <math.h>

#include "bitmap.h"

struct bitmap * bit_create(int size)
{
        struct bitmap * map = (struct bitmap *) malloc(sizeof(struct bitmap));
        /* If we can't fit the num of desired bits evenly we need to add another
           int bit will be partially used */
        map->num_words = (size / BITS_PER_WORD) + (((size % BITS_PER_WORD) > 0) ? 1 : 0);
        map->num_bits = size;
        map->map = calloc(map->num_words, sizeof(unsigned int));

        return map;
}

void bit_destroy(struct bitmap * map)
{
        free(map->map);
        free(map);
}

#ifndef FAST_BITMAP
int inline check_bounds(struct bitmap * map, int bit)
{
        return (map && (bit >= 0) && (bit < map->num_bits));
}
#endif

int bit_set(struct bitmap * map, int bit)
{
        #ifndef FAST_BITMAP
        if (check_bounds(map, bit))
                return -1;
        #endif

        map->map[bit / BITS_PER_WORD] = map->map[bit / BITS_PER_WORD] | (1 << (bit % BITS_PER_WORD));
        return 0;
}

int bit_clear(struct bitmap * map, int bit)
{
        #ifndef FAST_BITMAP
        if (check_bounds(map, bit))
                return -1;
        #endif

        map->map[bit / BITS_PER_WORD] = map->map[bit / BITS_PER_WORD] & ~(1 << (bit % BITS_PER_WORD));
        return 0;
}

int bit_test(struct bitmap * map, int bit)
{
        #ifndef FAST_BITMAP
        if (check_bounds(map, bit))
                return 0;
        #endif

        if (map->map[bit / BITS_PER_WORD] & (1 << (bit % BITS_PER_WORD))) return 1;
        else    return 0;
}

int bit_find(struct bitmap * map)
{
		#ifndef FAST_BITMAP
		if (!map)
			return -1;
		#endif

        int i;
        for (i = 0; i < map->num_bits; i++) {
                if (!bit_test(map, i)) {
                        bit_set(map, i);
                        return i;
                }
        }

        return -1;
}

int bit_diff(struct bitmap * a, struct bitmap * b)
{
	if (!a || !b)
		return -1;

	if (a->num_bits != b->num_bits)
		return -1;

	int set = 0; /* num bits set*/

	int i;
	unsigned int c;
	unsigned int xor;

	for (i = 0; i < a->num_words; i++) {
		xor = a->map[i] ^ b->map[i];
		
		#if __x86_64__
			c =  ((xor & 0xfff) * 0x1001001001001ULL & 0x84210842108421ULL) % 0x1f;
			c += (((xor & 0xfff000) >> 12) * 0x1001001001001ULL & 0x84210842108421ULL) % 
				 0x1f;
			c += ((xor >> 24) * 0x1001001001001ULL & 0x84210842108421ULL) % 0x1f;
			set += c;
		#else
			/* Brian Kernighan's bit count twiddle */
			for (c = 0; xor; c++)
				xor &= xor - 1;
			set += c;
		#endif
	}
	
	return set;
}

int bit_numSet(struct bitmap * map)
{
	unsigned int set = 0;
	unsigned int c, v;
	int i;
	for (i = 0; i < map->num_words; i++) {
		#if __x86_64__
			v = map->map[i];
			c =  ((v & 0xfff) * 0x1001001001001ULL & 0x84210842108421ULL) % 0x1f;
			c += (((v & 0xfff000) >> 12) * 0x1001001001001ULL & 0x84210842108421ULL) % 
				 0x1f;
			c += ((v >> 24) * 0x1001001001001ULL & 0x84210842108421ULL) % 0x1f;
			set += c;
		#else
			/* Brian Kernighan's bit count twiddle */
			for (c = 0; map->map[i]; c++)
				map->map[i] &= map->map[i] - 1;
			set += c;
		#endif
	}

	return set;
}

int bit_allSet(struct bitmap * map)
{
	int i;
	int num_words = ceil((double)map->num_bits / (double)BITS_PER_WORD);
	for (i = 0; i < num_words - 1; i++) {
		if (map->map[i] != (unsigned int)-1)
			return 0;
	}
	
	int bits_valid = map->num_bits % BITS_PER_WORD;
	int mask = (unsigned int)-1;
	mask <<= bits_valid;
	if (bits_valid > 0)
		mask = ~mask;

	if (map->map[num_words - 1] != mask)
		return 0;

	return 1;
}

int bit_allClear(struct bitmap * map)
{
	int i;
	int num_words = ceil((double)map->num_bits / (double)BITS_PER_WORD);
	for (i = 0; i < num_words; i++) {
		if (map->map[i] != 0)
			return 0;
	}

	return 1;
}

#ifdef BITMAP_DEBUG
#include <stdio.h>

void bit_print(struct bitmap * map)
{
        int i;
        printf("Bitmap num bits: %d, num words: %d\n", map->num_bits, map->num_words);

        for (i = 0; i < map->num_bits; i++){
                if (bit_test(map, i)) printf("1");
                else printf("_");
        }

        printf("\n");
}
#endif
