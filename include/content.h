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

#endif // CONTENT_H_INCLUDED
