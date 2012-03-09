#ifndef CCNUD_LISTENER_H_INCLUDED_
#define CCNUD_LISTENER_H_INCLUDED_

#include <netinet/in.h>

///Creates the domain socket we listen on.
///Returns 0 on success, non-zero otherwise.
int ccnudl_init();

///Closes the domain socket and does other cleanup.
int ccnudl_close();

///Where the listener thread spins waiting for connections.
void * ccnudl_service(void * arg);

#endif //CCNUD_LISTENER_H_INCLUDED_
