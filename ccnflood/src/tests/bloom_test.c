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
#include "bloom_filter.h"
#include "net_lib.h"

int main()
{

	struct bloom * filter;

	int rv = ccnf_cs_summary(&filter);

	bloom_print(filter);
	if (rv == 0) {
		printf("SUCCESS\n");
	} else {
		printf("FAILURE\n");
	}

    exit(rv);
}
