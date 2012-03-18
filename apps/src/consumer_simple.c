#include <stdio.h>
#include <stdlib.h>

#include "ccnu.h"
#include "content_name.h"
#include "content.h"

void print_usage(char * exec)
{
	printf("usage: %s content_name\n", exec);
	printf("example: %s /test/file", exec);
}

int main(int argc, char ** argv)
{
	if (argc != 2) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	char * str = argv[1];

	struct content_obj * con;
	struct content_name * name = content_name_create(str);
	if (ccnu_retrieve(name, &con) == 0) {
		uint8_t * data = con->data;
		printf("Got content: %s\n", str);
		int b;
		for (b = 0; b < con->size; b++) {
			printf("%c", data[b]);
		}
		exit(EXIT_SUCCESS);		
	} else {
		fprintf(stderr, "Failed to retrieve content, did you run producer before consumer?\n");
		exit(EXIT_FAILURE);
	}
}
