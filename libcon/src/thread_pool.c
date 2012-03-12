#include "thread_pool.h"

static void * tpool_worker(void * arg);

typedef struct job_s {
	job_fun_t job;
	void * arg;
	bool_t use_arg_free;
	delete_t arg_free;
	bool_t add_rv;
	pthread_mutex_t * cj_mutex;
	pthread_cond_t * cj_cond;
	struct linked_list * completed_jobs;
} tpjob_t;

int tpool_create(thread_pool_t * pool, int n_threads)
{
	pool->threads = malloc(sizeof(pthread_t) * n_threads);
	pool->pool_size = n_threads;
	if (pthread_mutex_init(&pool->pj_mutex, NULL) != 0) return -1;
	if (pthread_cond_init(&pool->pj_cond, NULL) != 0) return -1;
	if (pthread_mutex_init(&pool->cj_mutex, NULL) != 0) return -1;
	if (pthread_cond_init(&pool->cj_cond, NULL) != 0) return -1;
	pool->pending_jobs = linked_list_init(NULL);
	pool->completed_jobs = linked_list_init(NULL);
	pool->shutdown = FALSE;

	pthread_mutex_lock(&pool->pj_mutex);
	int i;
	for (i = 0; i < n_threads; i++) {
		if (pthread_create(&pool->threads[i], NULL, tpool_worker, pool) != 0) {
			return -1;
		}
	}
	pthread_mutex_unlock(&pool->pj_mutex);

	return 0;
}

int tpool_shutdown(thread_pool_t * pool)
{
	pthread_mutex_lock(&pool->pj_mutex);
		pool->shutdown = TRUE;
		pthread_cond_broadcast(&pool->pj_cond);
	pthread_mutex_unlock(&pool->pj_mutex);

	int i;
	for (i = 0; i < pool->pool_size; i++) {
		pthread_join(pool->threads[i], NULL);
	}

	free(pool->threads);
	linked_list_delete(pool->pending_jobs);
	linked_list_delete(pool->completed_jobs);
	pthread_mutex_destroy(&pool->pj_mutex);
	pthread_cond_destroy(&pool->pj_cond);
	pthread_mutex_destroy(&pool->cj_mutex);
	pthread_cond_destroy(&pool->cj_cond);

	return 0;
}

int tpool_add_job(thread_pool_t * pool, job_fun_t call, void * arg, int opt, delete_t arg_free, completed_jobs_t * cjl)
{
	tpjob_t * job = (tpjob_t * ) malloc(sizeof(tpjob_t));
	job->job = call;
	job->arg = arg;
	job->use_arg_free = ((opt & TPOOL_FREE_ARG) == TPOOL_FREE_ARG) ? TRUE : FALSE;
	job->arg_free = arg_free;
	job->add_rv = ((opt & TPOOL_NO_RV) == TPOOL_NO_RV) ? FALSE : TRUE;
	if ((opt & TPOOL_USE_CJL) == TPOOL_USE_CJL) {
		job->cj_mutex = &cjl->mutex;
		job->cj_cond = &cjl->cond;
		job->completed_jobs = cjl->completed;
		job->add_rv = TRUE;
	} else {
		job->cj_mutex = &pool->cj_mutex;
		job->cj_cond = &pool->cj_cond;
		job->completed_jobs = pool->completed_jobs;
	}

	pthread_mutex_lock(&pool->pj_mutex);
		linked_list_append(pool->pending_jobs, job);
		pthread_cond_signal(&pool->pj_cond);
	pthread_mutex_unlock(&pool->pj_mutex);

	return 0;
}
#include <stdio.h>
static void * tpool_worker(void * arg)
{
	thread_pool_t * pool = (thread_pool_t * ) arg;
	pthread_mutex_lock(&pool->pj_mutex);
	while (!pool->shutdown) {
		while (pool->pending_jobs->len == 0) {
			pthread_cond_wait(&pool->pj_cond, &pool->pj_mutex);
			if (pool->shutdown) goto SHUTDOWN;
		}
		/* we have the mutex & len > 0 */
		tpjob_t * job = linked_list_remove(pool->pending_jobs, 0);
		if (!job) continue;
		pthread_mutex_unlock(&pool->pj_mutex);

		void * ret_val = (job->job)(job->arg);
		if (job->use_arg_free) job->arg_free(job->arg);

		if (job->add_rv) {
			pthread_mutex_lock(job->cj_mutex);
				linked_list_append(job->completed_jobs, ret_val);
				pthread_cond_signal(job->cj_cond);
			pthread_mutex_unlock(job->cj_mutex);
		}
		free(job);

		pthread_mutex_lock(&pool->pj_mutex);
	}

	SHUTDOWN:
	pthread_mutex_unlock(&pool->pj_mutex);
	pthread_exit(NULL);
}

int tpool_get_job(thread_pool_t * pool, void ** ret)
{
	pthread_mutex_lock(&pool->cj_mutex);
		while (pool->completed_jobs->len == 0) {
			pthread_cond_wait(&pool->cj_cond, &pool->cj_mutex);
		}
		*ret = linked_list_remove(pool->completed_jobs, 0);
	pthread_mutex_unlock(&pool->cj_mutex);

	return 0;
}

int tpool_cjl_get_job(completed_jobs_t * cjl, void ** ret)
{
	pthread_mutex_lock(&cjl->mutex);
		while (cjl->completed->len == 0) {
			pthread_cond_wait(&cjl->cond, &cjl->mutex);
		}
		*ret = linked_list_remove(cjl->completed, 0);
	pthread_mutex_unlock(&cjl->mutex);

	return 0;
}
