#ifndef CCNUD_H_INCLUDED
#define CCNUD_H_INCLUDED

struct ccnfd_msg {
    uint8_t type;
    uint32_t payload_size;
    uint8_t * payload;
};

struct ccnfd_packet {
    uint8_t type;
    void * packet;
};

/**
 * DOCUMENTATION of msg format for ccnfd IPC
 * This is the protocol implemented by ccnf.h. Use that header for initiating
 * IPC.
 *
 * structure  of PUBLISH msg payload:
 * publisherId : int
 * name_len : int
 * name : char[name_len]
 * timestamp : int
 * size : int
 * data : byte[size]
 *
 * structure of RETRIEVE msg payload:
 * name_len : int
 * name : char[name_len]
 *
 * structure of CS_SUMMARY_REQ msg payload:
 * dummy : int
 */

struct listener_args {
    /* the monitor to wake up the main daemon */
    pthread_mutex_t * lock;
    pthread_cond_t  * cond;
    /* the queue to share incoming messages */
    struct synch_queue * queue;
};

extern struct log * g_log;
extern uint32_t g_nodeId;

#endif // CCNUD_H_INCLUDED
