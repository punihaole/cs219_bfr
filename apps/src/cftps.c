#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ccnu.h"
#include "content_name.h"
#include "content.h"
#include "ts.h"

static void print_usage(char * argv_0)
{
	printf("cftps\n");
	printf("CCNU FTP server: serves specified file under given content name\n");
    printf("Usage: %s filename content name\n", argv_0);
    printf("Example: %s /example/path/to/file /example/content/name\n", argv_0);
}

int main(int argc, char ** argv)
{
	if (argc != 3) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	char * filename = argv[1];
	char * con_name = argv[2];
	char str[MAX_NAME_LENGTH];
	if (strlen(con_name) > MAX_NAME_LENGTH) {
		printf("Proposed content name > MAX_NAME_LENGTH\n");	
		exit(EXIT_FAILURE);
	}
	struct content_name * name = content_name_create(con_name);

	printf("opening %s...\n", filename);
	FILE * fp = fopen(filename, "rb");
	if (fp == NULL) {
		perror("failed to open %s.\n");
		exit(EXIT_FAILURE);
	}

	fseek(fp, 0, SEEK_END);
	int fileLen = ftell(fp);
	rewind(fp);
	int max_segment_size = ccnu_max_payload_size(name);
	int min_segments = ceil(fileLen / max_segment_size);
	//we have to go back and reserve more space in the payload for a longer segment name
	int segment_size = max_segment_size - 1 - ceil(log(min_segments) / log(10)) - 1;
	int num_segments = ceil((double)((double)fileLen / (double)segment_size));
	printf("num segments = %d, segment size <= %d, file size = %d\n", 
	       num_segments, segment_size, fileLen);

	struct timespec ts;
	ts_fromnow(&ts);

	//publish the first segment which is the index, tells us how many segments there are
	struct content_obj index;
	index.name = name;
	index.timestamp = ts.tv_sec;
	index.size = 2 * sizeof(uint32_t);
	index.data = malloc(index.size);
	uint32_t num_segment_u = num_segments;
	uint32_t file_size_u = fileLen;
	memcpy(index.data, &num_segment_u, sizeof(uint32_t));
	memcpy(index.data+sizeof(uint32_t), &file_size_u, sizeof(uint32_t));

	char buffer[2048];
	int n;
	int chunk_id = 0;
	int bytes_to_read;
	struct linked_list * chunks = linked_list_init(NULL);
	while (fileLen > 0) {
		if (fileLen >= segment_size) {
			bytes_to_read = segment_size;
		} else {
			bytes_to_read = fileLen;
		}
		n = fread(buffer, 1, bytes_to_read, fp);
		sprintf(str, "%s/%d", con_name, chunk_id);
		printf("Creating chunk %d (name = %s), size = %d\n", chunk_id, str, n);
	
		struct content_obj * chunk = malloc(sizeof(struct content_obj));
		chunk->name = content_name_create(str);
		chunk->size = n;
		chunk->data = malloc(n);
		chunk->timestamp = ts.tv_sec;
		memcpy(chunk->data, buffer, n);

		linked_list_append(chunks, chunk);

		fileLen -= n;
		chunk_id++;
	}

	if (ccnu_publishSeq(&index, chunks) != 0) {
		printf("Failed to publish segment!\n");
	}

	fclose(fp);

	exit(EXIT_SUCCESS);
}

