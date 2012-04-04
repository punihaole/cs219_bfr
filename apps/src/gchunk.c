#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <ctype.h>

#include "ccnu.h"
#include "ccnf.h"
#include "content_name.h"
#include "content.h"

typedef int (*retrieve_t)(struct content_name * , struct content_obj ** );

void print_usage(char * exec)
{
	printf("usage: %s content_name\n -f(optional)", exec);
	printf("specifying the -f option indicates flooding of interests");
	printf("example: %s /test/file", exec);
}

int main(int argc, char ** argv)
{
	if (argc > 3) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	char * str = argv[1];
	retrieve_t retrieve_content;
	if (argc == 3 && (strcmp(argv[2], "-f") == 0)) {
		printf("Using interest flooding\n");
		retrieve_content = ccnf_retrieve;
	} else {
		printf("Using BFR\n");
		retrieve_content = ccnu_retrieve;
	}

	
	struct timeval tv_start, tv_end;

	struct content_obj * con;
	struct content_name * name = content_name_create(str);
	printf("Retrieving %s...\n", str);
	gettimeofday(&tv_start, NULL);
	if (retrieve_content(name, &con) == 0) {
		gettimeofday(&tv_end, NULL);
		uint8_t * data = con->data;
		printf("Got content: %s\n", str);
		int b;
		for (b = 0; b < con->size; b++) {
			if (isprint(data[b]))
				printf("%c", data[b]);
			else
				printf("%d", data[b]);
		}
		
		double time_start = tv_start.tv_sec + (double) tv_start.tv_usec / 1000000.0;
		double time_end = tv_end.tv_sec + (double) tv_end.tv_usec / 1000000.0;

		printf("\nTook %f sec.\n", time_end - time_start);
		exit(EXIT_SUCCESS);		
	} else {
		fprintf(stderr, "Failed to retrieve content, did you run producer before consumer?\n");
		exit(EXIT_FAILURE);
	}
}
