/**
 * key_list.h
 *
 * This module represents a key list. This is a list that cooresponds a key
 * with a list node.
 *
 * Data is retrieved by finding the list node with the same key supplied for
 * the desired data.
 **/

#ifndef KEY_LIST_H_INCLUDED
#define KEY_LIST_H_INCLUDED

#include "constants.h"

struct key_list;

/**
 * key_list_create
 * @param key_fun() - the function used to delete a node's key.
 * @param data_fun() - the function used to delete a node's data.
 * @param comp_fun() - the function used to compare two keys.
 *
 **/

struct key_list * key_list_create(delete_t key_fun, delete_t data_fun, compare_key_t comp_fun);

int key_list_delete(struct key_list * list);

int key_list_insert(struct key_list * list, void * key, void * data);

void * key_list_get(struct key_list * list, void * key);

int key_list_remove(struct key_list * list, void * key);

#endif // KEY_LIST_H_INCLUDED
