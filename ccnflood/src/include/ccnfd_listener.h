#ifndef CCNUD_LISTENER_H_INCLUDED_
#define CCNUD_LISTENER_H_INCLUDED_

#include <netinet/in.h>

///Creates the domain socket we listen on.
///Returns 0 on success, non-zero otherwise.
int ccnfdl_init(int pipeline_size);

///Closes the domain socket and does other cleanup.
int ccnfdl_close();

///Where the listener thread spins waiting for connections.
void * ccnfdl_service(void * arg);

#endif //CCNUD_LISTENER_H_INCLUDED_
