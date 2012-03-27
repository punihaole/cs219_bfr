#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#include "content.h"
#include "ccnf_packet.h"
#include "synch_queue.h"
#include "content_name.h"
#include "ccnf.h"
#include "net_lib.h"

int ccnf_net_test()
{
    struct content_obj obj;
    obj.name = content_name_create("/home/tom/test");
    obj.timestamp = 0;
    obj.data = (uint8_t * ) "hi there";
    obj.size = strlen("hi there");

    int rv = ccnf_publish(&obj);
    return rv;
}