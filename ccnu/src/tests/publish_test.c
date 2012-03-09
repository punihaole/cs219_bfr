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
	content.size = 1;
	content.timestamp = 1234567;
	content.data = malloc(sizeof(uint8_t) * 1);
	content.data[0] = 0xdd;

	int rv;
	rv = ccnu_publish(&content);
	if (!rv) {
		printf("SUCCESS\n");
	} else {
		printf("FAILED\n");
	}

	exit(rv);
}
