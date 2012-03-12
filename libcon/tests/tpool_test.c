#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "thread_pool.h"

void * is_prime(int * candidate)
{
	int n = *candidate;
	int * result = malloc(sizeof(int));

	if (n == 2) {
		//printf("%d is prime\n", n);
		*result = 1;
	} else if (((n % 2) == 0) || (n < 2)) {
		//printf("%d is not prime\n", n);
		*result = 0;
	} else {
		int i;
		for (i = 3; i < (int) sqrt((double)n); i += 2) {
			if ((n % i) == 0) {
				//printf("%d is not prime\n", n);
				*result = 0;
				return result;
			}
		}
		//printf("%d is prime\n", n);
		*result = 1;
	}
	return result;
}

int main()
{
	thread_pool_t pool;
	if (tpool_create(&pool, 8) < 0) exit(EXIT_FAILURE);

	int iterations = 10000000;
	int i;
	int * arg = malloc(sizeof(int) * iterations);
	int one_percent = iterations / 100;
	int next_cycle = 0;
	char cycle[] = "|\\-/";
	fprintf(stderr, "adding jobs               ");
	for (i = 0; i < iterations; i++) {
		if ((i % one_percent) == 0) {
			fprintf(stderr, "\b%c", cycle[next_cycle]);
			next_cycle = (1 + next_cycle) % 4;
			fflush(stderr);
		}
		arg[i] = i;
		tpool_add_job(&pool, (job_fun_t)is_prime, &arg[i], 0, NULL, NULL);
	}

	int primes = 0;
	
	fprintf(stderr, "\rgetting results           ");
	for (i = 0; i < iterations; i++) {
		int * p;
		tpool_get_job(&pool, (void *) &p);
		if ((i % one_percent) == 0) {
			fprintf(stderr, "\b%c", cycle[next_cycle]);
			next_cycle = (1 + next_cycle) % 4;
			fflush(stderr);
		}
		primes += *p;
		free(p);
	}
	printf("\n\rfound %d primes.\n", primes);
	
	printf("\nshutdown...\n");
	exit(tpool_shutdown(&pool));
}
