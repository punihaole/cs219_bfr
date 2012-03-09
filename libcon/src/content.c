#include <stdlib.h>

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
