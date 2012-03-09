#ifndef LINKED_LIST_H_INCLUDED
#define LINKED_LIST_H_INCLUDED

#include "constants.h"

struct linked_list_node {
    void * data;
    struct linked_list_node * prev, * next;
};

struct linked_list {
    struct linked_list_node * head, * tail;
    int len;
    delete_t del_fun;
};

struct linked_list * linked_list_init(delete_t del_fun);

void linked_list_delete(struct linked_list * list);

void linked_list_append(struct linked_list * list, void * data);

void linked_list_add(struct linked_list * list, void * data, unsigned index);

void * linked_list_remove(struct linked_list * list, unsigned index);

void * linked_list_get(struct linked_list * list, unsigned index);

#endif // LINKED_LIST_H_INCLUDED
