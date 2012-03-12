#include <string.h>
#include <stdio.h>

#include "bitmap.h"

int main()
{
	struct bitmap * map = bit_create(1000);
	map->num_bits = 20;
	int i;	
	for (i = 0; i < 20; i++)
		bit_find(map);
	if ((bit_allSet(map) == 1) && (bit_allClear(map) == 0))
		printf("success\n");
	else
		printf("failed %x\n", map->map[map->num_words-1]);

	map->num_bits = 21;
	printf("allSet = %d\n", bit_allSet(map));

	map = bit_create(16);
	for (i = 0; i < 16; i++)
		bit_set(map, i);
	if ((bit_allSet(map) == 1) && (bit_allClear(map) == 0))
		printf("success\n");
	else
		printf("failed %x\n", map->map[map->num_words-1]);

	map = bit_create(32);
	for (i = 0; i < 32; i++)
		bit_set(map, i);
	if ((bit_allSet(map) == 1) && (bit_allClear(map) == 0))
		printf("success\n");
	else
		printf("failed %x\n", map->map[map->num_words-1]);

	map = bit_create(64);
	for (i = 0; i < 64; i++)
		bit_set(map, i);
	if ((bit_allSet(map) == 1) && (bit_allClear(map) == 0))
		printf("success\n");
	else
		printf("failed %x\n", map->map[map->num_words-1]);

	map = bit_create(99);
	for (i = 0; i < 99; i++)
		bit_set(map, i);
	if ((bit_allSet(map) == 1) && (bit_allClear(map) == 0))
		printf("success\n");
	else
		printf("failed %x\n", map->map[map->num_words-1]);

	struct bitmap * a = bit_create(128);
	struct bitmap * b = bit_create(128);

	a->map[0] = 0x00000000;
	a->map[1] = 0x00000000;
	a->map[2] = 0x00000000;
	a->map[3] = 0x00000000;
	b->map[0] = 0xffffffff;
	b->map[1] = 0xffffffff;
	b->map[2] = 0xffffffff;
	b->map[3] = 0xffffffff;
	
	printf("diff = %d\n", bit_diff(a, b));

	a->map[0] = 0xffffffff;
	a->map[1] = 0xffffffff;
	a->map[2] = 0xffffffff;
	a->map[3] = 0xffffffff;
	b->map[0] = 0xffffffff;
	b->map[1] = 0xffffffff;
	b->map[2] = 0xffffffff;
	b->map[3] = 0xffffffff;

	printf("diff = %d\n", bit_diff(a, b));

	a->map[0] = 0x00000000;
	a->map[1] = 0x00000000;
	a->map[2] = 0x00000000;
	a->map[3] = 0x00000000;
	b->map[0] = 0x00000000;
	b->map[1] = 0x00000000;
	b->map[2] = 0x00000000;
	b->map[3] = 0x00000000;

	printf("diff = %d\n", bit_diff(a, b));

	a->map[0] = 0x12345678;
	a->map[1] = 0x90123456;
	a->map[2] = 0x78901234;
	a->map[3] = 0x56789012;
	b->map[0] = 0x12345678;
	b->map[1] = 0x90123456;
	b->map[2] = 0x78901234;
	b->map[3] = 0x56789012;

	printf("diff = %d\n", bit_diff(a, b));

	a->map[0] = 0x12345678;
	a->map[1] = 0x90123456;
	a->map[2] = 0x78901234;
	a->map[3] = 0x56789012;
	b->map[0] = ~a->map[0];
	b->map[1] = ~a->map[1];
	b->map[2] = ~a->map[2];
	b->map[3] = ~a->map[3];

	printf("diff = %d\n", bit_diff(a, b));

	a->map[0] = 0x00000007;
	a->map[1] = 0x00000007;
	a->map[2] = 0x00000007;
	a->map[3] = 0x00000007;
	b->map[0] = 0x00000003;
	b->map[1] = 0x00000005;
	b->map[2] = 0x00000001;
	b->map[3] = 0x00000000;

	printf("diff = %d\n", bit_diff(a, b));
}
