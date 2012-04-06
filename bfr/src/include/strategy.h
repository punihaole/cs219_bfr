/**
 * strategy.h
 *
 * Where we implement the routing loop. We spin and peridoically send updates,
 * parse received messages, etc...
 *
 **/

#ifndef STRATEGY_H_INCLUDED
#define STRATEGY_H_INCLUDED

#define JOIN_TIMEOUT_MS 100 /* ms */
#define JOIN_MAX_ATTEMPTS 4

int strategy_init(unsigned num_levels, int bloom_interval_ms, int cluster_interval_ms);

void strategy_close();

void * strategy_service(void * ignore);

/* the daemon filters out net messages that are not routing messages. The ones
 * the strategy layer needs to parse are added to our own queue here.
 */
int strategy_passMsg(struct bfr_msg * msg);

#endif // STRATEGY_H_INCLUDED
