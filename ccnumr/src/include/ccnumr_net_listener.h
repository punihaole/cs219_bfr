/**
 *
 * Listens to routing specific messages on a net domain port (currently 9988).
 * The routing messages should be broadcasted UDP.
 *
 **/

#ifndef CCNUMR_NET_LISTENER_H_INCLUDED
#define CCNUMR_NET_LISTENER_H_INCLUDED

#include "synch_queue.h"

/* Creates the net socket we listen on.
 * Returns 0 on success, non-zero otherwise.
 */
int ccnumr_net_listener_init();

/* Closes the net socket and does other cleanup. */
void ccnumr_net_listener_close();

/* listener thread spins here */
void * ccnumr_net_listener_service(void * _arg);

#endif /* CCNUMR_NET_LISTENER_H_INCLUDED */
