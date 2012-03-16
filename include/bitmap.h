/*
 * bitmap.h
 *
 */

#ifndef BITMAP_H_INCLUDED
#define BITMAP_H_INCLUDED

#define BITS_PER_BYTE 8
#define BITS_PER_WORD 32

#define FAST_BITMAP /* skip bounds checking */
#define BITMAP_DEBUG

struct bitmap {
        unsigned int * map;
        int num_bits; /* bits in the map */
        int num_words; /* convenience */
};

/// Creates a bit map with given size.
struct bitmap * bit_create(int size);

/// Destroys a given bit map.
void bit_destroy(struct bitmap * map);

/// Sets the given bit of the given bit map. Returns 0 on success.
int bit_set(struct bitmap * map, int bit);

/// Clears the given bit of the bit map. Returns 0 on success.
int bit_clear(struct bitmap * map, int bit);

/// Checks whether the given bit of the given bit map is set.
int bit_test(struct bitmap * map, int bit);

/// Finds a clear bit and sets it, returns its # or -1 if full.
int bit_find(struct bitmap * map);

/// Calculates the number of bits differing in 2 bitmap or -1 on failure.
int bit_diff(struct bitmap * a, struct bitmap * b);

/// Calculates the number of bits set in the bitmap
int bit_numSet(struct bitmap * map);

/// Returns 1 if all the bits are set
int bit_allSet(struct bitmap * map);

/// Returns 1 if all the bits are clear
int bit_allClear(struct bitmap * map);

/// For debugging
#ifdef BITMAP_DEBUG
void bit_print(struct bitmap * map);
#endif

#endif // BITMAP_H_INCLUDED
