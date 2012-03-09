/*
 * content_name_fixed.c
 */

#include <stdlib.h>
#include <string.h>

#include "content_name_fixed.h"

struct content_name_fixed * create_content_name_fixed(const char * name)
{
        struct component * temp, * curr;
        struct content_name_fixed * content;
        char * tok;
        int len;

        if (name == NULL)
                return NULL;

        content = (struct content_name_fixed *) malloc(sizeof(struct content_name_fixed));
        len = strlen(name);
        len = (len >= MAX_NAME_LENGTH) ? MAX_NAME_LENGTH : len;
        strncpy(content->full_name, name, len);
        content->len = len;

        char str[MAX_NAME_LENGTH];
        strncpy(str, content->full_name, len);

        tok = strtok(str, "/");
        if (tok == NULL)
                return NULL;

        content->head = (struct component *) malloc(sizeof(struct component));
        len = strlen(tok);
        content->head->name = (char *) malloc(len);
        strcpy(content->head->name, tok);
        content->head->len = len;
        content->head->next = NULL;
        curr = content->head;

        tok = strtok(NULL, "/");
        while (tok != NULL) {
                temp = (struct component*) malloc(sizeof(struct component));
                len = strlen(tok);
                temp->name = (char *) malloc(len);
                strcpy(temp->name, tok);
                temp->len = len;
                temp->next = NULL;

                curr->next = temp;
                curr = temp;
                content->tail = temp;

                tok = strtok(NULL, "/");
        }

        return content;
}

void content_name_fixed_delete(struct content_name_fixed * name)
{
        free(name->full_name);
        struct component * curr = name->head, * next;

        while (curr) {
                next = curr->next;
                free(curr->name);
                free(curr);
                curr = next;
        }

        free(name);
}

// tricky because we need to extend and copy the old name.
int content_name_fixed_append(struct content_name_fixed * name, char * comp)
{
        if (name == NULL || name->head == NULL || comp == NULL)
                return 1;

        int old_len = name->len;
        int comp_len = strlen(comp);

        comp_len = (comp_len + old_len >= MAX_NAME_LENGTH) ? MAX_NAME_LENGTH - old_len : comp_len;

        if (comp_len <= 1) {
            return 1;
        }

        name->full_name[name->len++] = NAME_COMPONENT_SEPARATOR;
        memcpy(&name->full_name[name->len], comp, comp_len);

        struct component * temp;
        /* the new tail */
        temp = (struct component * ) malloc(sizeof(struct component));
        temp->name = (char * ) malloc(comp_len);
        memcpy(temp->name, comp, comp_len);
        temp->next = NULL;
        name->tail->next = temp; /* update the old tail's next ptr */
        name->tail = temp; /* make us the new tail */
        name->len += comp_len;

        return 0;
}

char * content_name_fixed_get_component(struct content_name_fixed * name, int n)
{
        int i;
        struct component * curr;

        if (name == NULL || name->head == NULL)
                return NULL;

        curr = name->head;

        for (i = 0; i < n && curr->next != NULL; i++) {
                curr = curr->next;
        }
        return curr->name;
}

/**
 *
 * Procedure:
 * Start at first component of each name (the content name and the prefix to
 * test). The prefix must be fully contained in the name without differences.
 * Loop: compare the current component of each name
 * If the components are not the same length or any bytes differ stop.
 * Else increment matches and move to next component
 * Repeat
 *
 * TODO: Look for a faster way of doing this?
 * Have some ideas using index hashing tables and trees but will need to
 * investigate if that stuff is worthwhile.
 **/
int content_name_fixed_prefix_match(struct content_name_fixed * name, struct content_name_fixed * prefix)
{
        int matches = 0, i = 0;
        int stop = 0, end = 1;

        struct component * left = name->head;
        struct component * right = prefix->head;

        while (left && right) {
                for (i = 0; !end; i++) {
                        if (left->name[i] == '\0'){
                                stop = 1;
                                end = 1;
                        }
                        if (right->name[i] == '\0') {
                                stop = stop ^ 1;
                                end = 1;
                        }
                        if (left->name[i] != right->name[i]) {
                                return 0;
                        }
                }

                if (stop) {
                        break;
                }

                i = 0;
                end = 0;
                matches++;

                left = left->next;
                right = right->next;

                /* we cant match a name to a longer prefix */
                /* i.e. /a/b/c can't route to /a/b/c/d */
                if (right && !left) {
                        return 0;
                }
        }

        return matches;
}

#ifdef CONTENT_NAME_FIXED_DEBUG
#include <stdio.h>

void content_name_fixed_print_components(struct component * list)
{
        int i = 0;
        struct component* curr = list;

        while (curr != NULL) {
                printf("%s ", curr->name);
                curr = curr->next;
                i++;
        }
        printf("\n");
}
#endif

