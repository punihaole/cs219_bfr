#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "ccnu.h"
#include "ccnf.h"
#include "content_name.h"
#include "content.h"

typedef int (*retrieve_t)(struct content_name * , struct content_obj ** );
typedef int (*retrieveSeq_t)(struct content_name * , int, int, struct content_obj ** );

static void print_usage(char * argv_0)
{
	printf("cftp\n");
	printf("CCNU FTP client: downloads specified content name.\n");
	printf("Usage: %s contentname filename\n", argv_0);
	printf("Example: %s /example/content/name /example/path/to/file\n", argv_0);
}

int main(int argc, char ** argv)
{
	if (argc > 4 || argc < 3) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	retrieve_t retrieve_content;
	retrieveSeq_t retrieve_seq;
	if (argc == 4 && (strcmp(argv[3], "-f") == 0)) {
		printf("Using interest flooding\n");
		retrieve_content = ccnf_retrieve;
		retrieve_seq = ccnf_retrieveSeq;
	} else {
		printf("Using interest BFR\n");
		retrieve_content = ccnu_retrieve;
		retrieve_seq = ccnu_retrieveSeq;
	}

	struct timeval tv_start, tv_end;

	char * con_name = argv[1];
	char * filename = argv[2];
	if (strlen(con_name) > MAX_NAME_LENGTH) {
		printf("Proposed content name > MAX_NAME_LENGTH\n");	
		exit(EXIT_FAILURE);
	}

	struct content_name * name = content_name_create(con_name);
	struct content_obj * index;

	printf("retrieving index (%s).\n", con_name);
	gettimeofday(&tv_start, NULL);
	if (retrieve_content(name, &index) != 0) {
		printf("could not retrieve index segment!\n");
		exit(EXIT_FAILURE);
	}
	uint32_t num_segments_u;
	uint32_t file_size_u;
	memcpy(&num_segments_u, index->data, sizeof(uint32_t));
	memcpy(&file_size_u, index->data+sizeof(uint32_t), sizeof(uint32_t));
	int num_segments = (int)num_segments_u;
	int file_size = (int)file_size_u;

	printf("retrieved index, found %d segments, file size = %d.\n", num_segments, file_size);

	struct content_obj * obj;
	if (retrieve_seq(name, num_segments, file_size, &obj) != 0) {
		fprintf(stderr, "failed to retrieve segments!\n");
		exit(EXIT_FAILURE);
	}

	printf("finished retrieving content, %s.\n", obj->name->full_name);
	printf("\tpublisher = %u\n", obj->publisher);
	printf("\ttimestamp = %u\n", obj->timestamp);
	printf("\tsize = %u\n", obj->size);
	FILE * fp = fopen(filename, "wb");
	int bytes_written = 0;
	while (bytes_written < file_size) {
		int n = fwrite(obj->data+bytes_written, 1, file_size - bytes_written, fp);
		bytes_written += n;
	}
	fclose(fp);

	gettimeofday(&tv_end, NULL);
	double time_start = tv_start.tv_sec + (double) tv_start.tv_usec / 1000000.0;
	double time_end = tv_end.tv_sec + (double) tv_end.tv_usec / 1000000.0;

	printf("Took %f sec.\n", time_end - time_start);
	exit(EXIT_SUCCESS);
}

