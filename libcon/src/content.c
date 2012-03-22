#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "content.h"

void content_obj_destroy(struct content_obj * content)
{
    content_name_delete(content->name);
    content->name = NULL;
    free(content->data);
    content->data = NULL;
    free(content);
    content = NULL;
}

int content_is_segmented(struct content_name *  name)
{
	char * last_component = content_name_getComponent(name, name->num_components - 1);
	char * component = last_component;
	while (*component) {
        if (!isdigit(*component++)) {
            return 0;
        }
    }

	return 1;
}

int content_seq_no(struct content_name * name)
{
	char * last_component = content_name_getComponent(name, name->num_components - 1);
	return atoi(last_component);
}

char * content_prefix(struct content_name * name)
{
	char * last_component = content_name_getComponent(name, name->num_components - 1);
	int stlen = name->len - 1 - strlen(last_component);
	char * prefix = malloc(stlen + 1);
	strncpy(prefix, name->full_name, stlen);
	prefix[stlen] = '\0';
	return prefix;
}
