/**
 *
  *This module is as efficient as possible, that is we don't acquire the lock
 * until the last minute, and we release it immediately, making this as
 * efficient and parallel as possible.
 *
 **/

#include <pthread.h>
#include <stdlib.h>

#include "synch_queue.h"

struct synch_queue {
	struct q_node * head;
	struct q_node * tail;
	pthread_mutex_t * lock;
	int size;
};

struct q_node {
	struct q_node * next;
	void * data;
};

struct synch_queue * synch_queue_init()
{
    struct synch_queue * queue = (struct synch_queue * )
        malloc(sizeof(struct synch_queue));
    if (!queue)
        return NULL;

	queue->lock = malloc(sizeof(pthread_mutex_t));
	if (!queue->lock) {
        free(queue);
	    return NULL;
	}

	pthread_mutex_init(queue->lock, NULL);
	queue->head = queue->tail = NULL;
	queue->size = 0;

	return queue;
}

void synch_queue_delete(struct synch_queue * q)
{
    if (!q) return;

    pthread_mutex_destroy(q->lock);
    free(q->lock);
}

int synch_enqueue(struct synch_queue * queue, void * data)
{
    if (!queue || !data) return -1;

	struct q_node * node = (struct q_node * ) malloc(sizeof(struct q_node));
	if (!node) return -1;
	/* create the new tail */
	node->data = data;
	node->next = NULL;

	pthread_mutex_lock(queue->lock);
	/* update the old tail so he points to us */
	if (!queue->head) {
	     /* the queue must be empty */
        queue->head = node;
	} else {
        queue->tail->next = node;
	}

	/* now make us the new queue tail */
	queue->tail = node;
	queue->size++;
	pthread_mutex_unlock(queue->lock);

	return 0;
}

void * synch_dequeue(struct synch_queue * queue)
{
	void * data;
	struct q_node * first;

	if (!queue) return NULL;

	pthread_mutex_lock(queue->lock);
	first = queue->head;
	if (!first) {
	    pthread_mutex_unlock(queue->lock);
	    return NULL;
	}

	/* update the head pointer */
	queue->head = first->next;
	queue->size--;
	pthread_mutex_unlock(queue->lock);

	data = first->data;

	/* cleanup */
	free(first);

	return data;
}

int synch_len(struct synch_queue * q)
{
    if (!q)
        return 0;
    int len = 0;
    pthread_mutex_lock(q->lock);
        len = q->size;
    pthread_mutex_unlock(q->lock);

    return len;
}
