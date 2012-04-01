#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "bfrd.h"
#include "bfr_stats.h"
#include "log.h"

static struct log * stat_log;
static pthread_mutex_t stat_lock = PTHREAD_MUTEX_INITIALIZER;

int bfrstat_init(char * filename)
{
    stat_log = malloc(sizeof(struct log));
    char log_name[256];
    snprintf(log_name, 256, "bfr_stats_%u", g_bfr.nodeId);
    if (log_init(log_name, filename, stat_log, LOG_OVERWRITE)) return -1;

    return 0;
}

void bfrstat_rcvd_bloom(struct bloom_msg * msg)
{
    if (!msg) return;

    int bytes = HDR_SIZE + BLOOM_MSG_MIN_SIZE + (msg->vector_bits / 8);
    pthread_mutex_lock(&stat_lock);
    log_print(stat_log, "EVENT BLOOM_RCVD %d", bytes);
    pthread_mutex_unlock(&stat_lock);
}

void bfrstat_sent_bloom(struct bloom_msg * msg)
{
    if (!msg) return;

    int bytes = HDR_SIZE + BLOOM_MSG_MIN_SIZE + (msg->vector_bits / 8);
    pthread_mutex_lock(&stat_lock);
    log_print(stat_log, "EVENT BLOOM_SENT %d", bytes);
    pthread_mutex_unlock(&stat_lock);
}

void bfrstat_rcvd_join(struct cluster_join_msg * msg)
{
    if (!msg) return;

    int bytes = HDR_SIZE + CLUSTER_JOIN_MSG_SIZE;
    pthread_mutex_lock(&stat_lock);
    log_print(stat_log, "EVENT JOIN_RCVD %d", bytes);
    pthread_mutex_unlock(&stat_lock);
}

void bfrstat_sent_join(struct cluster_join_msg * msg)
{
    if (!msg) return;

    int bytes = HDR_SIZE + CLUSTER_JOIN_MSG_SIZE;
    pthread_mutex_lock(&stat_lock);
    log_print(stat_log, "EVENT JOIN_SENT %d", bytes);
    pthread_mutex_unlock(&stat_lock);
}

void bfrstat_rcvd_cluster(struct cluster_msg * msg)
{
    if (!msg) return;

    int bytes = HDR_SIZE + CLUSTER_RESPONSE_MSG_SIZE;
    pthread_mutex_lock(&stat_lock);
    log_print(stat_log, "EVENT CLUSTER_RCVD %d", bytes);
    pthread_mutex_unlock(&stat_lock);
}

void bfrstat_sent_cluster(struct cluster_msg * msg)
{
    if (!msg) return;

    int bytes = HDR_SIZE + CLUSTER_RESPONSE_MSG_SIZE;
    pthread_mutex_lock(&stat_lock);
    log_print(stat_log, "EVENT CLUSTER_SENT %d", bytes);
    pthread_mutex_unlock(&stat_lock);
}
