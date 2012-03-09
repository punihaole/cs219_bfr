/**
 * ccnud_net.h
 *
 * This modules sends CCNU interests and data.
 *
 **/

#ifndef CCNUD_NET_INCLUDED
#define CCNUD_NET_INCLUDED

#include <netinet/in.h>

#include "ccnud_constants.h"
#include "ccnu_packet.h"
#include "content.h"

extern struct synch_queue g_pak_queue;

int ccnudnb_init();

int ccnudnb_close();

/* used to specify options for the ccnudnb_express_interest function */
#define CCNUDNB_USE_ROUTE   0x1
#define CCNUDNB_USE_RETRIES 0x2
#define CCNUDNB_USE_TIMEOUT 0x4
#define CCNUDNB_USE_TTL     0x8
typedef struct ccnudnb_options {
	int mode;
	double distance;
    unsigned orig_level_u, orig_clusterId_u;
    unsigned dest_level_u, dest_clusterId_u;
    int retries;
    int timeout_ms;
    int ttl;
} ccnudnb_opt_t;

/* this is a blocking call that blocks until the data is received, so call
 * from within a thread that can sleep.
 * The opt parameter is optional. If use_opt = 0, opt may be NULL (since
 * it will be ignored). 
 * Make sure to set the mode mask of the opt parameter so that the parameters
 * specified will be used.
 * i.e. opt->mode = CCNUDNB_USE_ROUTE | CCNUDNB_USE_RETRIES | 
 *                  CCNUDNB_USE_TIMEOUT | CCNUDNB_USE_TTL;
 * will tell the function to use all the specified parameters.
 *
 * Any parameters that are not indicated to be used will be set to defaults
 * specified in the appropriate header file.
 *
 * CCNUDNB_USE_ROUTE - indicates to use the given distance, origin and dest-
 *                     ination clusters and not query the routing daemon for 
 *                     a route.
 * CCNUDNB_USE_RETRIES - indicates to use the retries parameter which sets
 *                       how many times to retry if an interest is not full-
 *                       filled within a timeout.
 * CCNUDNB_USE_TIMEOUT - indicates to use the timeout_ms parameter which sets
 *                       how long to wait for an interest to be fullfilled.
 * CCNUDNB_USE_TTL - indicates to use the ttl parameter which sets how many
 *                   hops an interest stays alive.
 */
int ccnudnb_express_interest(struct content_name * name, struct content_obj ** content_ptr,
                             int use_opt, struct ccnudnb_options * opt);

/* forwards an interest. We don't do anything clever, like update the routing
 * parameters or add PIT entry, etc. All we do here is take a interest and
 * put it on the wire.
 */
int ccnudnb_fwd_interest(struct ccnu_interest_pkt * interest);

/* forwards a data packet. Just dumps it on the wire. Clever stuff sould be
 * done elsewhere.
 */
int ccnudnb_fwd_data(struct content_obj * content, int hops_taken);

#endif /* CCNUD_NET_INCLUDED */
