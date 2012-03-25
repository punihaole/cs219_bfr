#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <math.h>

#include "ccnu.h"
#include "hash.h"
#include "content.h"
#include "net_lib.h"

int main()
{
	struct content_obj content;
	content.name = content_name_create("/tom/test");
	content.size = strlen("123tom test data123");
	content.timestamp = 1234567;
	content.data = malloc(content.size);
	strncpy((char * )content.data, "123tom test data123", content.size);

	int rv;
	ccnf_publish(&content);

	struct content_obj * retrieve;
	rv = ccnf_retrieve(content.name, &retrieve);
	if (!rv) {
		printf("SUCCESS\n");
	} else {
		printf("FAILED\n");
	}

	printf("retrieved %s\n", retrieve->name->full_name);
	printf("- timestamp = %d\n", retrieve->timestamp);
	printf("- data size = %d\n", retrieve->size);
	printf("- data = %s\n", retrieve->data);

	exit(rv);
}
