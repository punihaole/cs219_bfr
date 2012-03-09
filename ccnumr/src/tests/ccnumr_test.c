#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <math.h>

#include "ccnumr_test.h"
#include "ccnumr.h"
#include "ccnumrd.h"
#include "ccnumr_listener.h"
#include "ccnumr_net_listener.h"
#include "hash.h"
#include "grid.h"
#include "bloom_filter.h"
#include "linked_list.h"

int gridTest_2level();
int netTest_lib();
int bit_test1();
int linked_list_test();
int bloom_search_test();
int bloom_vector_test();

int test_suite()
{
    if (gridTest_2level() != 0) {
        fprintf(stderr,"Failed.\n");
    } else {
        fprintf(stderr,"Success.\n");
    }

    if (netTest_lib() != 0) {
        fprintf(stderr,"Failed.\n");
    } else {
        fprintf(stderr,"Success.\n");
    }

    if (bit_test1() != 0) {
        fprintf(stderr,"Failed.\n");
    } else {
        fprintf(stderr,"Success.\n");
    }

    if (linked_list_test() != 0) {
        fprintf(stderr,"Failed.\n");
    } else {
        fprintf(stderr,"Success.\n");
    }

    if (bloom_search_test() != 0) {
        fprintf(stderr,"Failed.\n");
    } else {
        fprintf(stderr,"Success.\n");
    }

    if (bloom_vector_test() != 0) {
        fprintf(stderr,"Failed.\n");
    } else {
        fprintf(stderr,"Success.\n");
    }

    return 0;
}

int gridTest_2level()
{
    fprintf(stderr,"Grid Test");

    grid_init(2, 100, 100);

    unsigned int x = 0, y = 0;

    /* test for level = 2 */
    int cluster;
    for (cluster = 0; cluster < 3; cluster++) {
        for (x = (cluster*25)+1; x <= ((cluster+1)*25); x++) {
            for (y = 0; y <= 25; y++) {
                if (grid_cluster(2,x,y) != cluster) {
                    printf("expected: %d, got %d cluster = %d, x = %d, y = %d\n", cluster, grid_cluster(2,x,y), cluster, x, y);
                    return -1;
                }
            }
        }
    }
    fprintf(stderr," . ");

    for (cluster = 0; cluster < 3; cluster++) {
        for (x = (cluster*25)+1; x <= ((cluster+1)*25); x++) {
            for (y = 26; y <= 50; y++) {
                if (grid_cluster(2,x,y) != cluster + 4) {
                    printf("expected: %d, got %d cluster = %d, x = %d, y = %d\n", cluster, grid_cluster(2,x,y), cluster, x, y);
                    return -1;
                }
            }
        }
    }
    fprintf(stderr," . ");

    for (cluster = 0; cluster < 3; cluster++) {
        for (x = (cluster*25)+1; x <= ((cluster+1)*25); x++) {
            for (y = 51; y <= 75; y++) {
                if (grid_cluster(2,x,y) != cluster + 8) {
                    printf("expected: %d, got %d cluster = %d, x = %d, y = %d\n", cluster, grid_cluster(2,x,y), cluster, x, y);
                    return -1;
                }
            }
        }
    }
    fprintf(stderr," . ");

    for (cluster = 0; cluster < 3; cluster++) {
        for (x = (cluster*25)+1; x <= ((cluster+1)*25); x++) {
            for (y = 76; y <= 100; y++) {
                if (grid_cluster(2,x,y) != cluster + 12) {
                    printf("expected: %d, got %d cluster = %d, x = %d, y = %d\n", cluster, grid_cluster(2,x,y), cluster, x, y);
                    return -1;
                }
            }
        }
    }
    fprintf(stderr," . ");

    /* test for level 1 */
    for (cluster = 0; cluster < 2; cluster++) {
        for (x = (cluster*50)+1; x <= ((cluster+1)*50); x++) {
            for (y = 0; y <= 50; y++) {
                if (grid_cluster(1,x,y) != cluster) {
                    printf("expected: %d, got %d cluster = %d, x = %d, y = %d\n", cluster, grid_cluster(2,x,y), cluster, x, y);
                    return -1;
                }
            }
        }
    }
    fprintf(stderr," . ");

    for (cluster = 0; cluster < 2; cluster++) {
        for (x = (cluster*50)+1; x <= ((cluster+1)*50); x++) {
            for (y = 51; y <= 100; y++) {
                if (grid_cluster(1,x,y) != cluster + 2) {
                    printf("expected: %d, got %d cluster = %d, x = %d, y = %d\n", cluster, grid_cluster(2,x,y), cluster, x, y);
                    return -1;
                }
            }
        }
    }
    fprintf(stderr," . ");

    return 0;
}

#include <limits.h>
#include <stdint.h> // defines uintN_t types
#include <inttypes.h> // defines PRIx macros
#include "net_lib.h"

int netTest_lib()
{
    fprintf(stderr,"NetLib Test");

    uint8_t buf[8];
    long l = 404998882888L;

    putLong(buf, l);
    long res = getLong(buf);

    if (l != res)
        return -1;

    float f = 3.1415926, f2;
    double d = 3.14159265358979323, d2;
    uint32_t fi;
    uint64_t di;

    fi = pack_ieee754_32(f);
    f2 = unpack_ieee754_32(fi);

    fprintf(stderr," . ");

    di = pack_ieee754_64(d);
    d2 = unpack_ieee754_64(di);

    fprintf(stderr," . ");

    if (f != f2)
        return -1;

    if (d != d2)
        return -1;

    putInt(buf, fi);
    double hf = unpack_ieee754_32(getInt(buf));

    fprintf(stderr," . ");

    if (f != hf)
        return -1;

    putLong(buf, di);
    double hd = unpack_ieee754_64(getLong(buf));

    if (d != hd)
        return -1;

    fprintf(stderr," . ");

    putByte(buf, 0xfe);
    uint8_t c = getByte(buf);

    if (c != 0xfe)
        return -1;

    fprintf(stderr," . ");

    return 0;
}

#include "bitmap.h"

int bit_test1()
{
    fprintf(stderr, "Bitmap Test");
    struct bitmap * map = bit_create(11);

    //bit_print(map);
    bit_set(map, 0);
    //bit_print(map);
    bit_clear(map, 0);
    //bit_print(map);
    bit_set(map, 10);
    //bit_print(map);
    bit_clear(map, 10);

    fprintf(stderr," . ");

    //bit_print(map);
    bit_set(map, 0);
    bit_set(map, 1);
    bit_set(map, 5);
    bit_set(map, 9);
    bit_set(map, 10);
    //bit_print(map);

    fprintf(stderr," . ");

    if ((bit_test(map, 10) | (bit_test(map, 9) << 1) | (bit_test(map, 8) << 2) |
        (bit_test(map, 7) << 3) | (bit_test(map, 6) << 4) | (bit_test(map, 5) << 5) |
        (bit_test(map, 4) << 6) | (bit_test(map, 3) << 7) | (bit_test(map, 2) << 8) |
        (bit_test(map, 1) << 9) | (bit_test(map, 0) << 10)) == 1571) return 0; //11000100011
    else return -1;
}

int linked_list_test()
{
    fprintf(stderr, "Linked List Test . . . . ");
    struct linked_list * list = linked_list_init(NULL);

    int * got = (int * )linked_list_get(list, 0);
    if (got) return -1;

    int * removed = linked_list_remove(list, -1);
    if (removed) return -1;

    if (linked_list_remove(list, 0) != NULL)
        return -1;
    if (linked_list_remove(list, 1) != NULL)
        return -1;
    if (linked_list_remove(list, -1) != NULL)
        return -1;

    int i = 9988;
    linked_list_append(list, &i);

    if (i != *((int *)linked_list_get(list, 0)))
        return -1;
    if (i != *((int *)linked_list_get(list, 1)))
        return -1;

    removed = linked_list_remove(list, 0);

    if (!removed || *removed != i) return -1;

    got = (int * )linked_list_get(list, 0);
    if (got) return -1;

    for (i = 0; i < 10; i++) {
        int * x = (int*) malloc(sizeof(int));
        *x = i;
        linked_list_add(list, x, i);
    }

    if (9 != *((int *)linked_list_get(list, 10000)))
        return -1;

    int * temp = (int * ) malloc(sizeof(int));
    *temp = 82888;
    linked_list_add(list, temp, 5);
    if (0 != *((int *)linked_list_get(list, 0)))
        return -1;
    if (1 != *((int *)linked_list_get(list, 1)))
        return -1;
    if (2 != *((int *)linked_list_get(list, 2)))
        return -1;
    if (3 != *((int *)linked_list_get(list, 3)))
        return -1;
    if (4 != *((int *)linked_list_get(list, 4)))
        return -1;
    if (82888 != *((int *)linked_list_get(list, 5)))
        return -1;
    if (5 != *((int *)linked_list_get(list, 6)))
        return -1;
    if (6 != *((int *)linked_list_get(list, 7)))
        return -1;
    if (7 != *((int *)linked_list_get(list, 8)))
        return -1;
    if (8 != *((int *)linked_list_get(list, 9)))
        return -1;
    if (9 != *((int *)linked_list_get(list, 10)))
        return -1;
    if (9 != *((int *)linked_list_get(list, 11)))
        return -1;
    if (9 != *((int *)linked_list_get(list, -1)))
        return -1;

    return 0;
}

static int search_bloom(struct bloom * filter, struct content_name * name)
{
    /* we iteratively check the content name in the bloom filter */
    char str[MAX_NAME_LENGTH];
    str[0] = '\0';

    int i = 1, matches = 0;
    struct component * curr = name->head;
    /* no longer using the content_name_getComponent func. Doing this by hand
     * is more flexible
     */
    char * copyFrom;
    copyFrom = name->full_name;
    while (curr != NULL) {
        strncat(str, copyFrom, curr->len+1);
        copyFrom += curr->len + 1;
        if (bloom_check(filter, str) == 1)
            matches = i;
        i++;
        curr = curr->next;
    }

    return matches; /* return num component matches */
}

#include "bloom_filter.h"
#include "hash.h"

int bloom_search_test()
{
    fprintf(stderr,"Bloom Search Test . . . ");
    struct bloom *filter = bloom_create(2048, 5, elfhash, sdbmhash, djbhash, dekhash, bphash);

    char * str1 = "/tom/test";
    bloom_add(filter, str1);
    char * str2 = "/tom/test/web";
    bloom_add(filter, str2);

    if (bloom_check(filter, "/tom/test") != 1)
        return -1;

    struct content_name * name = content_name_create("/tom/test");
    int matches = 0;
    if ((matches = search_bloom(filter, name)) != 2) {
        fprintf(stderr, "Expected 2 matches but got %d", matches);
        return -1;
    }
    content_name_delete(name);
    name = content_name_create("/tom/test/a");
    matches = 0;
    if ((matches = search_bloom(filter, name)) != 2) {
        fprintf(stderr, "Expected 2 matches but got %d", matches);
        return -1;
    }
    content_name_delete(name);
    name = content_name_create("/tom/test/web/home.html");
    matches = 0;
    if ((matches = search_bloom(filter, name)) != 3) {
        fprintf(stderr, "Expected 3 matches but got %d", matches);
        return -1;
    }
    content_name_delete(name);

    return 0;
}

int bloom_vector_test()
{
    fprintf(stderr,"Bloom Vector Test . . . ");
    struct bloom *filter = bloom_create(2048, 5, elfhash, sdbmhash, djbhash, dekhash, bphash);

    char * str1 = "/tom/test";
    bloom_add(filter, str1);
    char * str2 = "/tom/test/web";
    bloom_add(filter, str2);

    uint32_t * vector = (uint32_t * ) malloc(filter->vector->num_words * 4);
    memcpy(vector, filter->vector->map, filter->vector->num_words * 4);
    int bits = (filter->vector->num_words * 4) * 8;
    struct bloom * filter2 = bloom_createFromVector(bits, vector, 5, elfhash, sdbmhash, djbhash, dekhash, bphash);

    int i;
    for (i = 0; i < 2048/32; i++) {
        if (filter2->vector->map[i] != filter->vector->map[i])
            return -1;
    }

    return 0;
}
