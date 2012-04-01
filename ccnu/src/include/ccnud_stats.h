#ifndef CCNUD_STATS_H_INCLUDED
#define CCNUD_STATS_H_INCLUDED

#include "ccnu_packet.h"
#include "content.h"

/* initializes the file to write to */
int ccnustat_init(char * filename);
void ccnustat_done();

/* logs an interest reception event */
void ccnustat_rcvd_interest(struct ccnu_interest_pkt * interest);

/* logs an interest sent event */
void ccnustat_sent_interest(struct ccnu_interest_pkt * interest);

/* logs a data reception event */
void ccnustat_rcvd_data(struct ccnu_data_pkt * data);

/* logs a data sent event */
void ccnustat_sent_data(struct content_obj * content);

#endif // CCNUD_STATS_H_INCLUDED
