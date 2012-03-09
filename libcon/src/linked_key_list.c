#include "key_list.h"

#include <stdlib.h>

struct key_list_node {
        void * key;
        void * data;
        struct key_list_node * next;
};

struct key_list {
    struct key_list_node * head;
    struct key_list_node * tail;
    delete_t delete_key_fun;
    delete_t delete_data_fun;
    compare_key_t comp_fun;
};

struct key_list * key_list_create(delete_t key_fun, delete_t data_fun, compare_key_t comp_fun)
{
    struct key_list * list = (struct key_list * ) malloc(sizeof(struct key_list));
    if (!list) return NULL;

    list->head = NULL;
    list->tail = NULL;
    list->delete_key_fun = key_fun;
    list->delete_data_fun = data_fun;
    list->comp_fun = comp_fun;

    return list;
}

void delete_node(struct key_list * list, struct key_list_node * node)
{
    (*list->delete_key_fun)(node->key);
    (*list->delete_data_fun)(node->data);
}

int key_list_delete(struct key_list * list)
{
    if (!list) return -1;

    struct key_list_node * curr = list->head, * next;

    while (curr) {
        delete_node(list, curr);
        next = curr->next;
        free(curr);
        curr = next;
    }

    free(list);

    return 0;
}

int key_list_insert(struct key_list * list, void * key, void * data)
{
    if (!list || !key || !data) return -1;

    struct key_list_node * entry = (struct key_list_node * ) malloc(sizeof(struct key_list_node));
    if (!entry) return -1;

    entry->key = key;
    entry->data = data;
    entry->next = NULL;

    if (list->tail) {
        list->tail->next = entry;
        list->tail = entry;
    } else {
        /* we're adding the first node */
        list->head = entry;
        list->tail = entry;
    }

    return 0;
}

void * key_list_get(struct key_list * list, void * key)
{
    if (!list || !key) return NULL;

    struct key_list_node * curr = list->head;

    while (curr) {
        if ((*list->comp_fun)(curr->key, key) == 0) {
            return curr->data;
        }
        curr = curr->next;
    }

    return NULL;
}

int key_list_remove(struct key_list * list, void * key)
{
    if (!list || !key) return -1;

    struct key_list_node * curr = list->head, * last, * next;

    /* check if the head is the node to remove */
    if ((*list->comp_fun)(curr->key, key) == 0) {
        next = curr->next;
        if (!next) {
            /* we are in fact removing the only key */
            delete_node(list, curr);
            free(curr);
            list->head = list->tail = NULL;
            return 0;
        } else {
            delete_node(list, curr);
            free(curr);
            list->head = next;
        }
    }

    last = list->head;
    curr = curr->next;

    while (curr) {
        if ((*list->comp_fun)(curr->key, key) == 0) {
            /* delete this node */
            last->next = curr->next;
            if (curr == list->tail) {
                /* we deleted the last node and need to update the tail ptr */
                list->tail = last;
            }
            delete_node(list, curr);
            next = curr->next;
            free(curr);
            curr = next;
        } else {
            curr = curr->next;
        }
    }

    return 0;
}
