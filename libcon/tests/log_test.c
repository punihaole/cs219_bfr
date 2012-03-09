#include <string.h>
#include <stdio.h>

#include "log.h"
#include "net_lib.h"

int main()
{

	char log_name[256];
    snprintf(log_name, 256, "ccnud_%u", IP4_to_nodeId());
    char log_file[256];
	snprintf(log_file, 256, "ccnud_%u.log", IP4_to_nodeId());

	struct log * _log;
	
	log_init(log_name, log_file, &_log);

	log_print(_log, "print some numbers = %d, %d, and maybe a string, %s, yup.\n", 9, 82888, "tom");
	log_close(_log);
}
