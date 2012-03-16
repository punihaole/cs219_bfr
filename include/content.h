#ifndef CONTENT_H_INCLUDED
#define CONTENT_H_INCLUDED

#include <netinet/in.h>

#include "content_name.h"

struct content_obj {
	uint32_t publisher;
    struct content_name * name;
    int timestamp;
    int size;
    uint8_t * data;
};

void content_obj_destroy(struct content_obj * content);

/* returns 1 if the content_name ends in a .../x <- integer */
int content_is_segmented(struct content_name *  name);

/* returns the seq no of the name. should check if it is a
 * segmented content name first, as their is no invalid seq no */
int content_seq_no(struct content_name * name);

/* returns the prefix of a segmented content name */
char * content_prefix(struct content_name * name);

#endif // CONTENT_H_INCLUDED
