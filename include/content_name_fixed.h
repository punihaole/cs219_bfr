/* DEPRECATED */

/*
 * Content objects are named by a string that shall be treated as a sequence of
 * bytes because no string encoding has been defined. Trivially, any string en-
 * coding may be supported by treating the content name as a byte sequence.
 */

#ifndef CONTENT_NAME_FIXED_H_
#define CONTENT_NAME_FIXED_H_

#include "constants.h"
#include "content_name.h" /* component struct */

#define NAME_COMPONENT_SEPARATOR '/'

struct content_name_fixed {
        char full_name[MAX_NAME_LENGTH];
        int len;
        struct component * head; /* head of the component list */
        struct component * tail; /* tail of the comp. list */
};

/**
 * Creates a component list given a content name of the form: /comp1/comp2/.../compn
 * The str gets copied, so if the original string is freed, the content name is still okay
 **/
struct content_name_fixed * create_content_name_fixed(const char * name);

void content_name_fixed_delete(struct content_name_fixed * name);

/**
 * Creates a component struct from the string argument and appends it to the end
 * of the list by traversing down the components.
 * comp - the name of the component to add must not contain a '/'.
 * returns 0 on success.
 **/
int content_name_fixed_append(struct content_name_fixed * name, char * comp);

/**
 * n - the component you want -- we count from 0.
 * Returns the nth component or the last component if n > length of list.
 **/
char * content_name_fixed_get_component(struct content_name_fixed * name, int n);

/**
 * prefix_match
 * Returns the number of components that matched. Matches take place from left
 * to right. Matching ends as soon as a byte mismatches. The first component of
 * the name must match completely for a match to be considered possible.
 *
 * Returns:
 * 0 if no match (i.e. right argument is not a prefix of the left).
 * Number of components matching (the number of components in the prefix).
 **/
int content_name_fixed_prefix_match(struct content_name_fixed * name, struct content_name_fixed * prefix);

#ifdef CONTENT_NAME_FIXED_DEBUG
void content_name_fixed_print_components(struct component * list);
#endif

#endif /* CONTENT_NAME_FIXED_H_ */
