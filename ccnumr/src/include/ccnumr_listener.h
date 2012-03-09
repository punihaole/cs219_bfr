#ifndef CCNUMR_LISTENER_H_INCLUDED
#define CCNUMR_LISTENER_H_INCLUDED

/**
 *
 * Listens for IPC messages on the domain socket.
 *
 **/

#include "synch_queue.h"

/* Creates the domain socket we listen on.
 * Returns 0 on success, non-zero otherwise.
 */
int ccnumr_listener_init();

/* Closes the domain socket and does other cleanup. */
void ccnumr_listener_close();

/* listener thread spins here */
void * ccnumr_listener_service(void * _arg);

#endif /* CCNUMR_LISTENER_H_INCLUDED */
