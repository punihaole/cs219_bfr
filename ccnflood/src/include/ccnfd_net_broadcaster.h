/**
 * ccnfd_net.h
 *
 * This modules sends CCNU interests and data.
 *
 **/

#ifndef CCNUD_NET_INCLUDED
#define CCNUD_NET_INCLUDED

#include <netinet/in.h>

#include "ccnfd_constants.h"
#include "ccnf_packet.h"
#include "content.h"

extern struct synch_queue g_pak_queue;

int ccnfdnb_init();

int ccnfdnb_close();

/* used to specify options for the ccnfdnb_express_interest function */
#define CCNFDNB_USE_RETRIES        0x1
#define CCNFDNB_USE_TIMEOUT        0x2
#define CCNFDNB_USE_TTL            0x4
typedef struct ccnfdnb_options {
	int mode;
	double distance;
    unsigned orig_level_u, orig_clusterId_u;
    unsigned dest_level_u, dest_clusterId_u;
    int retries; /* number of times to send an interest */
    int timeout_ms;
    int ttl;
} ccnfdnb_opt_t;

/* this is a blocking call that blocks until the data is received, so call
 * from within a thread that can sleep.
 * The opt parameter is optional. If use_opt = 0, opt may be NULL (since
 * it will be ignored).
 * Make sure to set the mode mask of the opt parameter so that the parameters
 * specified will be used.
 * i.e. opt->mode = CCNFDNB_USE_RETRIES |
 *                  CCNFDNB_USE_TIMEOUT | CCNFDNB_USE_TTL;
 * will tell the function to use all the specified parameters.
 *
 * Any parameters that are not indicated to be used will be set to defaults
 * specified in the appropriate header file.
 *
 * CCNFDNB_USE_RETRIES - indicates to use the retries parameter which sets
 *                       how many times to retry if an interest is not full-
 *                       filled within a timeout.
 * CCNFDNB_USE_TIMEOUT - indicates to use the timeout_ms parameter which sets
 *                       how long to wait for an interest to be fullfilled.
 * CCNFDNB_USE_TTL - indicates to use the ttl parameter which sets how many
 *                   hops an interest stays alive.
 */
int ccnfdnb_express_interest(struct content_name * name, struct content_obj ** content_ptr,
                             int use_opt, struct ccnfdnb_options * opt);

/* forwards an interest. We don't do anything clever, like update the routing
 * parameters or add PIT entry, etc. All we do here is take a interest and
 * put it on the wire.
 */
int ccnfdnb_fwd_interest(struct ccnf_interest_pkt * interest);

/* forwards a data packet. Just dumps it on the wire. Clever stuff sould be
 * done elsewhere.
 */
int ccnfdnb_fwd_data(struct content_obj * content, int hops_taken);

#endif /* CCNUD_NET_INCLUDED */
