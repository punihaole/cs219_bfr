#include <sys/time.h>
#include <time.h>
#include <math.h>

#include "ts.h"

void ts_fromnow(struct timespec * ts)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	ts->tv_sec  = tv.tv_sec;
	ts->tv_nsec = tv.tv_usec * 1000;  
}

void ts_addns(struct timespec * ts, long ns)
{
	int sec = ns / 1000000000;
	ns = ns - sec * 1000000000;
	// we need to add the ns and adjust the time if overflow
	ts->tv_nsec += ns;

	ts->tv_sec += ts->tv_nsec / 1000000000 + sec;
	ts->tv_nsec = ts->tv_nsec % 1000000000;

}

void ts_addms(struct timespec * ts, long ms)
{
	int sec = ms / 1000;
	ms = ms - sec * 1000;

	// we need to add the ms (and convert to ns) and adjust the time if overflow
	ts->tv_nsec += ms * 1000000;

	ts->tv_sec += ts->tv_nsec / 1000000000 + sec;
	ts->tv_nsec = ts->tv_nsec % 1000000000;
}

void ts_adds(struct timespec * ts, long s)
{
	ts->tv_sec += s;
}

int ts_compare(struct timespec * a, struct timespec * b)
{
	if (a->tv_sec != b->tv_sec)
		return a->tv_sec - b->tv_sec;

	return a->tv_nsec - b->tv_nsec;
}

long ts_mselapsed(struct timespec * a, struct timespec * b)
{
	long sec_elapsed = abs(a->tv_sec - b->tv_sec);
	long ns_elapsed = abs(a->tv_nsec - b->tv_nsec);

	long ms_elapsed = ceil(ns_elapsed / 1000000.0) + (sec_elapsed * 1000);

	return ms_elapsed;
}

