/*
 * content_name.c
 */

#include <stdlib.h>
#include <string.h>

#include "content_name.h"

struct content_name * content_name_create(const char * name)
{
    struct component * temp, * curr;
    struct content_name * content;
    char * tok, str[MAX_NAME_LENGTH];
    int len;

    if (name == NULL)
        return NULL;

    // imperative that we prevent buffer overflow by nasty content names.
    strncpy(str, name, MAX_NAME_LENGTH);
    str[MAX_NAME_LENGTH-1] = 0;

    content = (struct content_name *) malloc(sizeof(struct content_name));
    len = strlen(str);
    content->full_name = (char *) malloc(len + 1);
    content->len = len;
    strcpy(content->full_name, str);

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
	content->num_components = 1;

    tok = strtok(NULL, "/");

    while (tok != NULL)
    {
        temp = (struct component*) malloc(sizeof(struct component));
        len = strlen(tok);
        temp->name = (char *) malloc(len);
        strcpy(temp->name, tok);
        temp->len = len;
        temp->next = NULL;

        curr->next = temp;
        curr = temp;

        tok = strtok(NULL, "/");

		content->num_components++;
    }

    return content;
}

void content_name_delete(struct content_name * name)
{
    if (!name) return;
    free(name->full_name);
    name->full_name = NULL;
    struct component * curr = name->head, * next;

    while (curr)
    {
        next = curr->next;
        free(curr->name);
        free(curr);
        curr = next;
    }

    free(name);
    name = NULL;
}

// tricky because we need to extend and copy the old name.
int content_name_appendComponent(struct content_name * name, char * comp)
{
    if (name == NULL || name->head == NULL || comp == NULL)
        return -1;

    int old_len = name->len;
    int comp_len = strlen(comp);
    int new_len = old_len + comp_len + 1; /* add 1 for new separator */

    new_len = (new_len >= MAX_NAME_LENGTH) ? MAX_NAME_LENGTH : new_len;

    if ((new_len == old_len) || ((new_len - 1) == old_len))
    {
        /* we don't have enough room for the new component or we only have
           room to add the '/', so we should just return error and stop.   */
        return -1;
    }

    char * save = (char * ) realloc(name->full_name, new_len);
    if (!save)
    {
        return -1;
    }

    name->full_name = save;
    name->full_name[old_len] = NAME_COMPONENT_SEPARATOR;
    name->full_name[old_len+1] = '\0'; /* mark where to concat */
    /* append the comp to full name string */
    comp_len = (comp_len + old_len >= MAX_NAME_LENGTH) ? MAX_NAME_LENGTH - old_len - 1 : comp_len;
    strncat(name->full_name, comp, comp_len);
    name->full_name[new_len] = '\0';
    name->len = new_len;

    struct component * temp, * curr;
    //the new tail
    temp = (struct component *) malloc(sizeof(struct component));
    temp->name = (char * ) malloc(comp_len);
    strcpy(temp->name, comp);
    temp->next = NULL;
	temp->len = comp_len;

    curr = name->head;
    while (curr->next != NULL)
        curr = curr->next;
    curr->next = temp;

	name->num_components++;

    return 0;
}

char * content_name_removeComponent(struct content_name * name, int n)
{
	int i;
    struct component * curr, * prev;

    if (name == NULL || name->head == NULL)
        return NULL;

    curr = name->head;
	
	int str_off = 0; //position in full_name
    for (i = 0; i < n && curr->next != NULL; i++)
    {
		str_off += curr->len + 1;
		prev = curr;
        curr = curr->next;
    }

	if (curr == name->head) {
		name->head = curr->next;	
	} else {
		prev->next = curr->next;
	}

	if (i == (name->num_components - 1)) {
		memset(name->full_name+str_off, 0, name->len-str_off);
	} else {
		char tail[MAX_NAME_LENGTH];
		strncpy(tail, name->full_name+str_off, name->len-str_off);
		strncpy(name->full_name+str_off, tail+curr->len+1, name->len-str_off-curr->len-1);
	}
	name->num_components--;
	name->len -= curr->len + 1;
	/* shrink the block size on heap -- this isn't totally necessary because
     * of the way append is implemented. We do this to free up the bytes
     * at the end of the block in the case an append is never made.
     */
	char * reallocd = realloc(name->full_name, name->len);
	if (reallocd) name->full_name = reallocd;
	char * save = curr->name;
	free(curr);
	
    return save;
}

char * content_name_getComponent(struct content_name * name, int n)
{
    int i;
    struct component * curr;

    if (name == NULL || name->head == NULL)
        return NULL;

    curr = name->head;

    for (i = 0; i < n && curr->next != NULL; i++)
    {
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
 * @TODO Look for a faster way of doing this?
 * Have some ideas using index hashing tables and trees but will need to
 * investigate if that stuff is worthwhile.
 **/
int content_name_prefixMatch(struct content_name * name, struct content_name * prefix)
{
    int matches = 0, i = 0;
    int stop = 0, end = 1;

    struct component * left = name->head;
    struct component * right = prefix->head;

    while (left && right)
    {
        for (i = 0; !end; i++)
        {
            if (left->name[i] == '\0')
            {
                stop = 1;
                end = 1;
            }
            if (right->name[i] == '\0')
            {
                stop = stop ^ 1;
                end = 1;
            }
            if (left->name[i] != right->name[i])
            {
                return 0;
            }
        }

        if (stop)
        {
            break;
        }

        i = 0;
        end = 0;
        matches++;

        left = left->next;
        right = right->next;

        //we cant match a name to a longer prefix
        //i.e. /a/b/c can't route to /a/b/c/d
        if (right && !left)
        {
            return 0;
        }
    }

    return matches;
}

#ifdef CONTENT_NAME_DEBUG
#include <stdio.h>

void content_name_printComponents(struct content_name * name)
{
    int i = 0;
    struct component * curr = name->head;

	printf("|");
    while (curr != NULL)
    {
        printf("%s:%d|", curr->name, curr->len);
        curr = curr->next;
        i++;
    }
    printf("\n");
}
#endif

