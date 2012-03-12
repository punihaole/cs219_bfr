#ifndef CONSTANTS_H_INCLUDED
#define CONSTANTS_H_INCLUDED

/* CONTENT */
#define MAX_NAME_LENGTH 128

typedef enum bool_enum {
	FALSE = 0,
	TRUE = 1
} bool_t;

typedef int (*compare_key_t)(void *, void *);
typedef unsigned int (*evict_t)(unsigned int *, unsigned int);
typedef void (*delete_t)(void *);
typedef int (*compare_t)(const char *, const char *, int len);

/* BLOOM/SUMMARY */
#define BLOOM_ARGS 5, elfhash, sdbmhash, djbhash, dekhash, bphash

#define HASH(n) (elfhash(n))

/* MISC */
#ifndef max
	#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
	#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef msleep
    #define msleep(n) (usleep((n) * 1000))
#endif

#endif // CONSTANTS_H_INCLUDED

