#include <stdio.h>
#include <stdlib.h>

#include "ccnumr.h"

void print_usage(char * exec)
{
	printf("usage: %s x y\n", exec);
	printf("example: %s 100.0 200.0", exec);
}

int main(int argc, char ** argv)
{
	if (argc != 3) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	double x = atof(argv[1]);
	double y = atof(argv[2]);

	if (ccnumr_sendLoc(x, y) == 0) {
		exit(EXIT_SUCCESS);		
	} else {
		fprintf(stderr, "Failed to send coordinates!\n");
		exit(EXIT_FAILURE);
	}
}
