#ifndef LOG_H_
#define LOG_H_

#include <stdio.h>
#include <pthread.h>

struct log {
	FILE * log_fp;
	char * log_name;
	int pending_writes;
	pthread_mutex_t lock;
	int level;
};

#define LOG_APPEND    1 /* we append to log file if exists */
#define LOG_OVERWRITE 2 /* we overwrite to log file if exists */
#define LOG_PRESERVE  4 /* we fail if the log file already exists */

/* LOG entries come in several flavors:
 * 1. INFO
 * 2. DEBUG
 * 3. WARNING
 * 4. ERROR
 * 5. CRITICAL
 * The logging level is a bitwise OR of the types of log entries you
 * wish to output.
 * LOG_NORMAL is a shortcut to output only warnings, errors and critical erros.
 * LOG_DEVEL is a shortcut to output everything.
 * You can craft your own log mode by bitwise oring any of the available modes.
 */
#define LOG_INFO      8
#define LOG_IMPORTANT 16
#define LOG_DEBUG     32
#define LOG_WARNING   64
#define LOG_ERROR     128
#define LOG_CRITICAL  256

#define LOG_NORMAL (LOG_IMPORTANT | LOG_WARNING | LOG_ERROR | LOG_CRITICAL)
#define LOG_DEVEL  (LOG_NORMAL | LOG_DEBUG | LOG_INFO)

int log_init(char * log_name, char * filename, struct log * _log_ptr, int mode);

int log_close(struct log * _log);

void log_flush(struct log * _log);

void log_print(struct log * _log, const char * format, ...);
void log_important(struct log * _log, const char * format, ...);
void log_debug(struct log * _log, const char * format, ...);
void log_warn(struct log * _log, const char * format, ...);
void log_error(struct log * _log, const char * format, ...);
void log_critical(struct log * _log, const char * format, ...);

void log_assert(struct log * _log, int expression, const char * format, ...);

#endif //LOG_H_
