#ifndef CCNUD_NET_LISTENER_H_INCLUDED
#define CCNUD_NET_LISTENER_H_INCLUDED

#include "synch_queue.h"
#include "ccnfd_pit.h"

/* Creates the net socket we listen on.
 * Returns 0 on success, non-zero otherwise.
 */
int ccnfdnl_init(int pipeline_size);

/* Closes the net socket and does other cleanup. */
int ccnfdnl_close();

/* listener thread spins here */
void * ccnfdnl_service(void * _arg);

void ccnfdnl_reg_segment(_segment_q_t * seg);

void ccnfdnl_unreg_segment(_segment_q_t * seg);

#endif // CCNUD_NET_LISTENER_H_INCLUDED
