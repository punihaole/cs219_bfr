#ifndef CCNFD_STATS_H_INCLUDED
#define CCNFD_STATS_H_INCLUDED

#include "ccnf_packet.h"
#include "content.h"

/* initializes the file to write to */
int ccnfstat_init(char * filename);

/* logs an interest reception event */
void ccnfstat_rcvd_interest(struct ccnf_interest_pkt * interest);

/* logs an interest sent event */
void ccnfstat_sent_interest(struct ccnf_interest_pkt * interest);

/* logs a data reception event */
void ccnfstat_rcvd_data(struct ccnf_data_pkt * data);
void ccnfstat_rcvd_data_unsolicited(struct ccnf_data_pkt * data);

/* logs a data sent event */
void ccnfstat_sent_data(struct content_obj * content);

#endif // CCNFD_STATS_H_INCLUDED
