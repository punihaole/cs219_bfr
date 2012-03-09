/**
 * synch_queue.h
 *
 * An implementation of a synchronized queue. We initialize a queue structure
 * with a queue unique mutex. This mutex is used to atomically append nodes to
 * the queue tail or poll them from the head.
 *
 **/

#ifndef SYNCH_QUEUE_H_INCLUDED_
#define SYNCH_QUEUE_H_INCLUDED_

#include <pthread.h>

struct synch_queue;

/* returns ptr to a new synchronized queue */
struct synch_queue * synch_queue_init();

/* only call this with an empty queue, unless you like memory leaks */
void synch_queue_delete(struct synch_queue * q);

/* atomically appends to the queue. returns 0 on success */
int synch_enqueue(struct synch_queue * q, void * data);
/* atomically removes the head of the queue.
 * returns ptr to first piece of data or NULL if empty */
void * synch_dequeue(struct synch_queue * q);

/* returns the length of the queue */
int synch_len(struct synch_queue * q);

#endif //SYNCH_QUEUE_H_INCLUDED_
