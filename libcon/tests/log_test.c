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

	struct log _log;
	
	log_init(log_name, log_file, &_log, LOG_OVERWRITE | LOG_NORMAL);

	log_print(&_log, "info");
	log_important(&_log, "important");
	log_debug(&_log, "debug");
	log_warn(&_log, "warn");
	log_error(&_log, "error");
	log_critical(&_log, "critical");

	log_close(&_log);
}
