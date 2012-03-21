#ifndef DISTANCE_TABLE_H_INCLUDED
#define DISTANCE_TABLE_H_INCLUDED

#define MAX_HOPS 10

int dtab_init(int size);

int dtab_getHops(char * name);

int dtab_setHops(char * name, int hops);

#endif // DISTANCE_TABLE_H_INCLUDED
