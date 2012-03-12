#ifndef LOG_H_
#define LOG_H_

#include <stdio.h>

struct log {
	FILE * log_fp;
	char * log_name;
	int pending_writes;
};

#define LOG_APPEND    1 /* we append to log file if exists */
#define LOG_OVERWRITE 2 /* we overwrite to log file if exists */
#define LOG_PRESERVE  4 /* we fail if the log file already exists */

int log_init(char * log_name, char * filename, struct log * _log_ptr, int mode);

int log_close(struct log * _log);

void log_print(struct log * _log, const char * str, ...);

void log_printnow(struct log * _log, const char * format, ...);

#endif //LOG_H_
