#ifndef TS_H_INCLUDED_
#define TS_H_INCLUDED_

#include <sys/time.h>
#include <time.h>

/* get the current time */
void ts_fromnow(struct timespec * ts);

/* add ns nanoseconds to the ts (ns > 0) */
void ts_addns(struct timespec * ts, long ns);

/* add ms milliseconds to the ts (ms > 0) */
void ts_addms(struct timespec * ts, long ms);

/* add s seconds to the ts (s > 0) */
void ts_adds(struct timespec * ts, long s);

/* compare a and b
 * returns 0 if a and b are the exact same time,
 * > 0 if a is later than b, and < 0 else.
 */
int ts_compare(struct timespec * a, struct timespec * b);

#endif //_TS_H_INCLUDED_
