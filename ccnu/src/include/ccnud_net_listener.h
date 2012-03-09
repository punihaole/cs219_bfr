#ifndef CCNUD_NET_LISTENER_H_INCLUDED
#define CCNUD_NET_LISTENER_H_INCLUDED

#include "synch_queue.h"
#include "ccnud_pit.h"

/* Creates the net socket we listen on.
 * Returns 0 on success, non-zero otherwise.
 */
int ccnudnl_init();

/* Closes the net socket and does other cleanup. */
int ccnudnl_close();

/* listener thread spins here */
void * ccnudnl_service(void * _arg);

#endif // CCNUD_NET_LISTENER_H_INCLUDED
