#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "ccnfd.h"
#include "ccnfd_stats.h"
#include "log.h"

static struct log * stat_log;
static pthread_mutex_t stat_lock = PTHREAD_MUTEX_INITIALIZER;

int ccnfstat_init(char * filename)
{
    stat_log = malloc(sizeof(struct log));
    char log_name[256];
    snprintf(log_name, 256, "ccnf_stats_%u", g_nodeId);
    if (log_init(log_name, filename, stat_log, LOG_OVERWRITE)) return -1;

    return 0;
}

void ccnfstat_rcvd_interest(struct ccnf_interest_pkt * interest)
{
    if (!interest) return;

    int bytes = MIN_INTEREST_PKT_SIZE + interest->name->len;
    pthread_mutex_lock(&stat_lock);
    log_print(stat_log, "EVENT INTEREST_RCVD %d", bytes);
    pthread_mutex_unlock(&stat_lock);
}

void ccnfstat_sent_interest(struct ccnf_interest_pkt * interest)
{
    if (!interest) return;
    int bytes = MIN_INTEREST_PKT_SIZE + interest->name->len;
    pthread_mutex_lock(&stat_lock);
    log_print(stat_log, "EVENT INTEREST_SENT %d", bytes);
    pthread_mutex_unlock(&stat_lock);
}

void ccnfstat_rcvd_data(struct ccnf_data_pkt * data)
{
    if (!data) return;
    int bytes = MIN_DATA_PKT_SIZE + data->name->len + data->payload_len;
    pthread_mutex_lock(&stat_lock);
    log_print(stat_log, "EVENT DATA_RCVD %d", bytes);
    pthread_mutex_unlock(&stat_lock);
}

void ccnfstat_sent_data(struct content_obj * content)
{
    if (!content) return;
    int bytes = MIN_DATA_PKT_SIZE + content->name->len + content->size;
    pthread_mutex_lock(&stat_lock);
    log_print(stat_log, "EVENT DATA_SENT %d", bytes);
    pthread_mutex_unlock(&stat_lock);
}
