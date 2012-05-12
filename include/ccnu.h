/**
 * ccnu.h
 *
 * This header gives the prototypes for functions useful for communicating with
 * the ccnud daemon.
 *
 **/

#ifndef CCNU_H_INCLUDED
#define CCNU_H_INCLUDED

#include "content.h"
#include "bloom_filter.h"
#include "content_name.h"
#include "linked_list.h"

#define MSG_IPC_PUBLISH        1
#define MSG_IPC_SEQ_PUBLISH    2
#define MSG_IPC_RETRIEVE       3
#define MSG_IPC_SEQ_RETRIEVE   4
#define MSG_IPC_CS_SUMMARY_REQ 5
#define MSG_IPC_TIMEOUT        6
#define MSG_IPC_RETRIES        7
#define MSG_IPC_RETRIEVE_OPT   8

/* used to specify options for the ccnudnb_express_interest function */
#define CCNUDNB_USE_ROUTE          0x1
#define CCNUDNB_USE_RETRIES        0x2
#define CCNUDNB_USE_TIMEOUT        0x4
#define CCNUDNB_USE_TTL            0x8
typedef struct ccnu_options {
	int mode;
	double distance;
    unsigned orig_level_u, orig_clusterId_u;
    unsigned dest_level_u, dest_clusterId_u;
    int retries; /* number of times to send an interest */
    int timeout_ms;
    int ttl;
} ccnu_opt_t;

inline void ccnu_did2sockpath(uint32_t daemonId, char * str, int size);

/**
 * ccnu_set_timeout
 *      Set the number of milliseconds to wait before retransmitting an
 *      interest.
 **/
int ccnu_set_timeout(unsigned timeout_ms);

/**
 * ccnu_set_retries
 *      Set the number of times to retransmit and interest before ccnu_retrieve
 *      fails.
 **/
int ccnu_set_retries(unsigned max_attempts);

/**
 * ccnu_max_payload_size
 *      Calculates the maximum allowable payload size from the maximum
 *      allowable packet size, the number of overhead bytes in a data
 *      packet and the number of overhead bytes in transmitting the
 *      content name. For now, applications must do segmentation.
 * Returns >= 0 on success (num of bytes of payload, allowed)
 **/
int ccnu_max_payload_size(struct content_name * name);

/**
 * ccnu_publish
 *      Publish a piece of content. This copies the content into the content
 *      store.
 * returns 0 on success
 **/
int ccnu_publish(struct content_obj * content);

/**
 * ccnu_publishSeq
 *      Publish a piece of segmented piece of content. The index_obj stores
 *      application specific metadata for the segment. The chunks list is a
 *      list of chunks making up the segment.
 *      The name of the index_obj is a prefix, which all the chunks should
 *      share (although we don't check this right now!). The chunks have
 *      an additional conent name component which is the sequence number.
 *      i.e. /prefix/0, /prefix/1, ... /prefix/N
 * returns 0 on success
 **/
int ccnu_publishSeq(struct content_obj * index_obj, struct linked_list * chunks);

/**
 * ccnu_retrieve
 *      Retrieves a piece of content. If it is not in the content store, an
 *      interest will be sent across the network and the caller will block.
 * returns 0 on success
 * The pointer to the content_obj pointer will point to a newly created and
 * populated content object.
 **/
int ccnu_retrieve(struct content_name * name, struct content_obj ** content_ptr);

/**
 * ccnu_retrieveSeq
 *      Retrieves a segmented piece of content. The number of chunks and the
 *      total file_len should be passed.
 *      The file will be stored in segments in the CS, but will be returned as
 *      a single content obj to the caller.
 **/
int ccnu_retrieveSeq(struct content_name * baseName, int chunks, int file_len,
                     struct content_obj ** content_ptr);

int ccnu_retrieveSeq_opts(struct content_name * baseName, int chunks, int file_len,
                     	  struct content_obj ** content_ptr, ccnu_opt_t * opts);
/**
 * ccnu_cs_summary
 *      Requests the ccnu daemon hash the content names in the CS and put their
 *      fingerprints into a Bloom filter.
 * returns 0 on success.
 * The pointer to the Bloom filter pointer will point to a newly created and
 * populated Bloom filter.
 */
int ccnu_cs_summary(struct bloom ** bloom_ptr);

#endif // CCNU_H_INCLUDED
