#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <ctype.h>

#include "bfr.h"
#include "content_name.h"
#include "content.h"

void print_usage(char * exec)
{
	printf("usage: %s -n content_name -c interval(optional)\n", exec);
	printf("Returns the route to a given content name.\n");
	printf("Specify -c to continously query the route every X sec.\n");
}

int main(int argc, char ** argv)
{
	if (argc < 2) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	char str[MAX_NAME_LENGTH];
	memset(str, 0, MAX_NAME_LENGTH);
	int loop = 0;
	int delay = 0;

	int c;
    while ((c = getopt(argc, argv, "-n:c:")) != -1) {
		switch (c) {
			case 'n':
				strncpy(str, optarg, MAX_NAME_LENGTH);
				break;
			case 'c':
				delay = atoi(optarg);
				if (delay == 0) {
					delay = 1;				
				}
				loop = 1;
				break;
			default:
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	struct content_name * con_name = content_name_create(str);

	unsigned orig_level;
	unsigned orig_clusterId;
	unsigned dest_level;
	unsigned dest_clusterId;
	double distance;

	do {
		int rv = bfr_sendWhere(con_name, &orig_level, &orig_clusterId,
				  &dest_level, &dest_clusterId, &distance);
		if (rv != 0) {
			break;
		}
		printf("route found (%u:%u) -> (%u:%u), distance = %5.5f.\n",
               orig_level, orig_clusterId, dest_level, dest_clusterId, 
               distance);
		sleep(delay);
	} while (loop);

	exit(EXIT_SUCCESS);
}
