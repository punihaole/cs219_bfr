#include "hash.h"

unsigned int elfhash(const char * str)
{
        unsigned int hash = 0;
        unsigned int x = 0;

        while (*str) {
                hash = (hash << 4) + (*str);
                if ((x = hash & 0xF0000000L) != 0) {
                        hash ^= (x >> 24);
                }
                hash &= ~x;
                str++;
        }

        return hash;
}

unsigned int sdbmhash(const char * str)
{
        unsigned int hash = 0;

        while(*str) {
                hash = (*str) + (hash << 6) + (hash << 16) - hash;
                str++;
        }

        return hash;
}

unsigned int djbhash(const char * str)
{
        unsigned int hash = 5381;

        while(*str) {
                hash = ((hash << 5) + hash) + (*str);
                str++;
        }

        return hash;
}

unsigned int dekhash(const char * str)
{
        unsigned int len = 0;
        while (str[len]) {
                len++;
        }
        unsigned int hash = len;

        while(*str) {
                hash = ((hash << 5) ^ (hash >> 27)) ^ (*str);
                str++;
        }
        return hash;
}

unsigned int bphash(const char* str)
{
        unsigned int hash = 0;

        while(*str) {
                hash = hash << 7 ^ (*str);
                str++;
        }

        return hash;
}
