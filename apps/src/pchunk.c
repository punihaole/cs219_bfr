#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "ccnu.h"
#include "ts.h"

void print_usage(char * exec)
{
	printf("%s publishes as many bytes of input as possible under the given content name\n", exec);
	printf("usage: %s content_name \"content\"\n", exec);
	printf("example: %s /hello_world \"Hello World!\"", exec);
	printf("example: cat Hello_World.txt | %s /hello_world -", exec);
}

int main(int argc, char ** argv)
{
	if (argc != 3) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	char * name = argv[1];
	char * data_src = argv[2];

	struct timespec now;
	ts_fromnow(&now);	
	
	struct content_obj con;
	con.name = content_name_create(name);
	con.timestamp = now.tv_sec;
	con.data = (uint8_t * ) malloc(ccnu_max_payload_size(con.name));
	
	char input[1024];
	int offset = 0;
	int can_write = ccnu_max_payload_size(con.name);
	if (strcmp(data_src, "-") == 0) {
		
		while (can_write > 0) {
			int n = read(0, input, 1024);
			if (n > 0) {
				int write = (n > can_write) ? can_write : n;
				memcpy(con.data+offset, input, write);
				can_write -= write;
				offset += write;
			} else break;
		}
		printf("wrote %d bytes\n", offset);
		con.size = offset;
	} else {
		int write = (strlen(data_src) > can_write) ? can_write : strlen(data_src);
		memcpy(con.data, data_src, write);
		con.size = write;
	}
	con.data = realloc(con.data, con.size);
	
	exit(ccnu_publish(&con));
}
