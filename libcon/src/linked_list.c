#include <stdlib.h>

#include "linked_list.h"

struct linked_list * linked_list_init(delete_t del_fun)
{
    struct linked_list * list = (struct linked_list *)
        malloc(sizeof(struct linked_list));
    list->len = 0;
    list->head = list->tail = NULL;
    list->del_fun = del_fun;

    return list;
}

void linked_list_delete(struct linked_list * list)
{
    int i;
    for (i = 0; i < list->len; i++) {
        (*list->del_fun)(linked_list_remove(list, 0));
    }
	free(list);
	list = NULL;
}

void linked_list_append(struct linked_list * list, void * data)
{
    struct linked_list_node * node = (struct linked_list_node *)
        malloc(sizeof(struct linked_list_node));
    node->data = data;
    node->prev = node->next = NULL;

    if (list->len == 0) {
            list->head = list->tail = node;
    } else {
        node->prev = list->tail;
        list->tail->next = node;
        list->tail = node;
    }

    list->len++;
}

void linked_list_add(struct linked_list * list, void * data, unsigned index)
{
    struct linked_list_node * node = (struct linked_list_node *)
        malloc(sizeof(struct linked_list_node));
    node->data = data;
    node->prev = node->next = NULL;

    if (index == 0) {
        if (list->len > 0) {
            list->head->prev = node;
            node->next = list->head;
            list->head = node;
        } else {
            free(node);
            return linked_list_append(list, data);
        }
    } else if (index >= (list->len - 1)) {
        free(node);
        return linked_list_append(list, data);
    } else {
        if (list->len == 1) {
            free(node);
            return linked_list_append(list, data);
        }

        int i;
        struct linked_list_node * insert_after = list->head,
                                * insert_before = list->head->next;
        for (i = 1; i < index; i++) {
            insert_after = insert_after->next;
            insert_before = insert_before->next;
        }

        insert_after->next = node;
        node->prev = insert_after;
        node->next = insert_before;
        insert_before->prev = node;
    }

    list->len++;
}

void * linked_list_remove(struct linked_list * list, unsigned index)
{
    if (index >= list->len) return NULL;

    void * data;
    struct linked_list_node * node = NULL;

    if (index == 0) {
        if (list->len == 1) {
            node = list->head;
            list->head = list->tail = NULL;
        } else {
            list->head->next->prev = NULL;
            node = list->head;
            list->head = list->head->next;
        }
    } else if (index == list->len - 1) {
        list->tail->prev->next = NULL;
        node = list->tail;
        list->tail = list->tail->prev;
    } else {
        node = list->head->next;
        int i = 1;
        while (node->next && i < index) {
            node = node->next;
            i++;
        }

		struct linked_list_node * before, * after;
		before = node->prev;
		after = node->next;
		before->next = after;
		after->prev = before;
    }

    list->len--;

    data = node->data;
    free(node);

    return data;
}

void * linked_list_get(struct linked_list * list, unsigned index)
{
    if (list->len == 0) return NULL;

    struct linked_list_node * node = list->head;
    int i = 0;
    while (i < index && node->next) {
        node = node->next;
        i++;
    }

    return node->data;
}

