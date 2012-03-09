#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ccnud_cs.h"
#include "content_name.h"
#include "content.h"
#include "hash.h"

int cs_test()
{
	CS_init(OLDEST, 0.02);

	//make sure its emtpy
	struct bloom * filter = NULL;
	CS_summary(&filter);
	bloom_print(filter);
	bloom_destroy(filter);

	struct content_obj obj;
	obj.name = content_name_create("/tom/test");
	obj.timestamp = 123;
	obj.size = 0;
	obj.data = NULL;

	CS_put(&obj);
	CS_summary(&filter);
	bloom_print(filter);
	bloom_destroy(filter);

	/*struct content_name * name = content_name_create("/tom");
	struct content_obj * co = CS_match(name);

	if (&obj != co) {
        if (co == NULL) printf("NULL\n");
		printf("failed!\n");
		exit(EXIT_FAILURE);
	}*/

    exit(EXIT_SUCCESS);
}
