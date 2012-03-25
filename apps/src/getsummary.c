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

#include "ccnf.h"
#include "ccnu.h"
#include "hash.h"
#include "bloom_filter.h"

int main(int argc, char ** argv)
{
	struct bloom * filter;
	int rv;

	if (argc == 2 && (strcmp(argv[1], "-f") == 0)) {
		rv = ccnf_cs_summary(&filter);
	} else {
		rv = ccnu_cs_summary(&filter);
	}

	if (rv == 0) {
		bloom_print(filter);
	}

    exit(rv);
}
