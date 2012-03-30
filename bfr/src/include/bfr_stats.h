#ifndef BFR_STATS_H_INCLUDED
#define BFR_STATS_H_INCLUDED

#include "bfr.h"

/* initializes the file to write to */
int bfrstat_init(char * filename);

void bfrstat_rcvd_bloom(struct bloom_msg * msg);

void bfrstat_sent_bloom(struct bloom_msg * msg);

void bfrstat_rcvd_join(struct cluster_join_msg * msg);

void bfrstat_sent_join(struct cluster_join_msg * msg);

void bfrstat_rcvd_cluster(struct cluster_msg * msg);

void bfrstat_sent_cluster(struct cluster_msg * msg);

#endif // BFR_STATS_H_INCLUDED
