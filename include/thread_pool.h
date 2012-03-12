#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_

#include <pthread.h>
#include <stdlib.h>

#include "constants.h"
#include "linked_list.h"

#define TPOOL_FREE_ARG  0x1 /* free the arg ptr */
#define TPOOL_NO_RV     0x2 /* do not write return value to completed_jobs */
#define TPOOL_USE_CJL   0x4 /* use a custom complete jobs list */
typedef void * (*job_fun_t) (void * );

typedef struct tpool {
	pthread_t * threads;
	int pool_size;
	pthread_mutex_t pj_mutex;
	pthread_cond_t pj_cond;
	pthread_mutex_t cj_mutex;
	pthread_cond_t cj_cond;
	struct linked_list * pending_jobs;
	struct linked_list * completed_jobs;
	bool_t shutdown;
} thread_pool_t;

typedef struct cjlist {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	struct linked_list * completed;
} completed_jobs_t;

/* creates a thread pool with  specified number of threads */
int tpool_create(thread_pool_t * pool, int n_threads);

/* this function shall be called only if a thread pool has completed.
 * May lose data if called while working threads are executing.
 */
int tpool_shutdown(thread_pool_t * pool);

/* add a job to the thread pool. This is a function to execute which has the given
 * prototype, and its argument.
 * Specify any options you might wish.
 * opt = TPOOL_FREE_ARG means that worker thread will free the argument passed
 * in after it complete the function. Do not set this bit if you wish to do free
 * the pointer elsewhere.
 * if you specify the TPOOL_FREE_ARG arg_free should be the function to use to
 * free the arg. If TPOOL_FREE_ARG is set but arg_free_fun is NULL, unspecified
 * behavior will follow.
 * If TPOOL_FREE_ARG is 0, arg_free_fun may (and should) be NULL, since it will
 * be ignored.
 * IF TPOOL_USE_CJL is 0, cjl will be ignored. Else, you should provide a cjl
 * structure which will be where the worker thread places the return value
 * before locking the mutex and sending a signal. This allows the caller to
 * separate their data from other caller's data if so desired.
 */
int tpool_add_job(thread_pool_t * pool, job_fun_t call, void * arg, int opt, delete_t arg_free_fun,
completed_jobs_t * cjl);

/* Polls the return value of a completed job stored only in the tpool completed jobs list.
 * Blocks if no jobs are finished yet. Every add_job call should be followed
 * with exactly 1 get_job call.
 */
int tpool_get_job(thread_pool_t * pool, void ** ret);

/* Pools a return value from a completed jobs struct. */
int tpool_cjl_get_job(completed_jobs_t * cjl, void ** ret);

#endif /* THREAD_POOL_H_ */

