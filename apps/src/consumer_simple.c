#include <stdio.h>
#include <stdlib.h>

#include "ccnu.h"
#include "content_name.h"
#include "content.h"

int main()
{
	struct content_obj * con;
	struct content_name * name = content_name_create("/test/hello");
	if (ccnu_retrieve(name, &con) == 0) {
		char * str = (char * ) con->data;
		printf("Got content: %s\n", str);
		exit(EXIT_SUCCESS);		
	} else {
		fprintf(stderr, "Failed to retrieve content, did you run producer before consumer?\n");
		exit(EXIT_FAILURE);
	}
}
