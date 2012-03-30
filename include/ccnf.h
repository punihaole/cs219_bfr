/**
 * ccnf.h
 *
 * This header gives the prototypes for functions useful for communicating with
 * the ccnfd daemon.
 *
 **/

#ifndef CCNF_H_INCLUDED
#define CCNF_H_INCLUDED

#include "content.h"
#include "bloom_filter.h"
#include "content_name.h"
#include "linked_list.h"
#include "ccnu.h"

inline void ccnf_did2sockpath(uint32_t daemonId, char * str, int size);

/**
 * ccnu_set_timeout
 *      Set the number of milliseconds to wait before retransmitting an
 *      interest.
 **/
int ccnf_set_timeout(unsigned timeout_ms);

/**
 * ccnu_set_retries
 *      Set the number of times to retransmit and interest before ccnu_retrieve
 *      fails.
 **/
int ccnf_set_retries(unsigned max_attempts);

/**
 * ccnf_max_payload_size
 *      Calculates the maximum allowable payload size from the maximum
 *      allowable packet size, the number of overhead bytes in a data
 *      packet and the number of overhead bytes in transmitting the
 *      content name. For now, applications must do segmentation.
 * Returns >= 0 on success (num of bytes of payload, allowed)
 **/
int ccnf_max_payload_size(struct content_name * name);

/**
 * ccnf_publish
 *      Publish a piece of content. This copies the content into the content
 *      store.
 * returns 0 on success
 **/
int ccnf_publish(struct content_obj * content);

/**
 * ccnf_publishSeq
 *      Publish a piece of segmented piece of content. The index_obj stores 
 *      application specific metadata for the segment. The chunks list is a
 *      list of chunks making up the segment.
 *      The name of the index_obj is a prefix, which all the chunks should
 *      share (although we don't check this right now!). The chunks have
 *      an additional conent name component which is the sequence number.
 *      i.e. /prefix/0, /prefix/1, ... /prefix/N 
 * returns 0 on success
 **/
int ccnf_publishSeq(struct content_obj * index_obj, struct linked_list * chunks);

/**
 * ccnf_retrieve
 *      Retrieves a piece of content. If it is not in the content store, an
 *      interest will be sent across the network and the caller will block.
 * returns 0 on success
 * The pointer to the content_obj pointer will point to a newly created and
 * populated content object.
 **/
int ccnf_retrieve(struct content_name * name, struct content_obj ** content_ptr);

/**
 * ccnf_retrieveSeq
 *      Retrieves a segmented piece of content. The number of chunks and the
 *      total file_len should be passed.
 *      The file will be stored in segments in the CS, but will be returned as
 *      a single content obj to the caller.
 **/
int ccnf_retrieveSeq(struct content_name * baseName, int chunks, int file_len, 
                     struct content_obj ** content_ptr);
/**
 * ccnf_cs_summary
 *      Requests the ccnu daemon hash the content names in the CS and put their
 *      fingerprints into a Bloom filter.
 * returns 0 on success.
 * The pointer to the Bloom filter pointer will point to a newly created and
 * populated Bloom filter.
 */
int ccnf_cs_summary(struct bloom ** bloom_ptr);

#endif // CCNF_H_INCLUDED
