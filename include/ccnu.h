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

#define MSG_IPC_PUBLISH        1
#define MSG_IPC_RETRIEVE       2
#define MSG_IPC_SEQ_RETRIEVE   3
#define MSG_IPC_CS_SUMMARY_REQ 4

inline void ccnu_did2sockpath(uint32_t daemonId, char * str, int size);

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
 * ccnu_retrieve
 *      Retrieves a piece of content. If it is not in the content store, an
 *      interest will be sent across the network and the caller will block.
 * returns 0 on success
 * The pointer to the content_obj pointer will point to a newly created and
 * populated content object.
 **/
int ccnu_retrieve(struct content_name * name, struct content_obj ** content_ptr);

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
