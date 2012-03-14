#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "ccnud_cs.h"
#include "content_name.h"
#include "content.h"
#include "hash.h"

static int is_num(char * component)
{
    while (*component) {
        if (!isdigit(*component++)) {
            return 0;
        }
    }
    return 1;
}

int cs_test()
{
	CS_init(OLDEST, 0.02);

	//make sure its emtpy
	/*struct bloom * filter = NULL;
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

	struct content_name * name = content_name_create("/tom");
	struct content_obj * co = CS_match(name);

	if (&obj != co) {
        if (co == NULL) printf("NULL\n");
		printf("failed!\n");
		exit(EXIT_FAILURE);
	}*/

    struct content_name * name = content_name_create("/music/0");
    char * last_component = content_name_getComponent(name, name->num_components - 1);
    if (!is_num(last_component)) {
        printf("not a num\n");
        exit(EXIT_SUCCESS);
    }
	char prefix[MAX_NAME_LENGTH];
	printf("name->len = %d, strlen(last_comp) = %d\n", name->len, (int)strlen(last_component));
    strncpy(prefix, name->full_name, name->len - 1 - strlen(last_component));
    prefix[name->len - 1 - strlen(last_component)] = '\0';
    printf("prefix = %s, name = %s\n", prefix, name->full_name);
    int chunk = atoi(last_component);
    printf("chunk = %d\n", chunk);

    exit(EXIT_SUCCESS);
}
