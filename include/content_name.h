/*
 * Content objects are named by a string that shall be treated as a sequence of
 * bytes because no string encoding has been defined.
 *
 * We enforce the maximum name size defined in constants.h.
 */

#ifndef CONTENT_NAME_H_
#define CONTENT_NAME_H_

#include "constants.h"

#define NAME_COMPONENT_SEPARATOR '/'
#define CONTENT_NAME_DEBUG

struct component {
        char * name;
        int len; /* len of the char * name */
        struct component * next;
};

struct content_name {
        char * full_name; /* treat as a sequence of bytes */
        int len; /* len of the full_name */
        struct component * head; /* head of the component list */
		int num_components;
};

/**
 * Creates a component list given a content name of the form: /comp1/comp2/.../compn
 **/
struct content_name * content_name_create(const char * name);

void content_name_delete(struct content_name * name);

/**
 * Creates a component struct from the string argument and appends it to the end
 * of the list by traversing down the components.
 * comp - the name of the component to add must not contain a '/'.
 * returns 0 on success.
 **/
int content_name_appendComponent(struct content_name * name, char * comp);

/**
 * n - the component you want -- we count from 0.
 * Returns the nth component or the last component if n > length of list.
 **/
char * content_name_getComponent(struct content_name * name, int n);

/**
 * n - the component to remove -- we count from 0.
 * Returns the nth component's string or the last component if n > length of list.
 **/
char * content_name_removeComponent(struct content_name * name, int n);

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
int content_name_prefixMatch(struct content_name * name, struct content_name * prefix);

#ifdef CONTENT_NAME_DEBUG
void content_name_printComponents(struct content_name * name);
#endif

#endif /* CONTENT_NAME_H_ */
