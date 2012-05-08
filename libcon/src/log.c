#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "log.h"

#define HEADER_LEN 256
#define MAX_PENDING_WRITES 0

int log_init(char * log_name, char * filename, struct log * _log, int mode)
{
	if (!log_name || !filename || !_log) return -1;

	if ((mode & LOG_PRESERVE) == LOG_PRESERVE) {
		FILE *fp = fopen(filename, "r");
		if (fp) {
			fprintf(stderr, "log %s already exists!\n", filename);
			fclose(fp);
			return -1;
		}
	} else if ((mode & LOG_APPEND) == LOG_APPEND) {
		_log->log_fp = fopen(filename, "a");
	} else	if ((mode & LOG_OVERWRITE) == LOG_OVERWRITE) {
		_log->log_fp = fopen(filename, "w");
	}

	if (_log->log_fp == NULL) return -1;

	_log->log_name = (char * ) malloc(strlen(log_name));
	strcpy(_log->log_name, log_name);
	_log->pending_writes = 0;
	pthread_mutex_init(&_log->lock, NULL);
	_log->level = mode;

	return 0;
}

int log_close(struct log * _log)
{
	if (!_log) return -1;

	fclose(_log->log_fp);
	free(_log->log_name);
	pthread_mutex_destroy(&_log->lock);
	_log = NULL;

	return 0;
}

void log_flush(struct log * _log)
{
	pthread_mutex_lock(&_log->lock);
	fflush(_log->log_fp);
	pthread_mutex_unlock(&_log->lock);
}

static inline void log_printlevel(struct log * _log, int level, const char * format, va_list ap)
{
	/* assume its a LOG_INFO */
	if (!_log || !format || !(level & _log->level)) return;

	struct tm * current;
	time_t now;

	time(&now);
	current = localtime(&now);

	char * date = asctime(current);
	date[strlen(date)-1] = '\0';

	pthread_mutex_lock(&_log->lock);
		fprintf(_log->log_fp, "%s %s: ", date, _log->log_name);
		vfprintf(_log->log_fp, format, ap);
		fprintf(_log->log_fp, "\n");
		_log->pending_writes++;

		if (_log->pending_writes > MAX_PENDING_WRITES) {
			fflush(_log->log_fp);
			_log->pending_writes = 0;
		}

	pthread_mutex_unlock(&_log->lock);
}

void log_print(struct log * _log, const char * format, ...)
{
	va_list ap;
	va_start(ap, format);
	log_printlevel(_log, LOG_INFO, format, ap);
	va_end(ap);
}

void log_debug(struct log * _log, const char * format, ...)
{
	va_list ap;
	va_start(ap, format);
	log_printlevel(_log, LOG_DEBUG, format, ap);
	va_end(ap);
}

void log_important(struct log * _log, const char * format, ...)
{
	va_list ap;
	va_start(ap, format);
	log_printlevel(_log, LOG_IMPORTANT, format, ap);
	va_end(ap);
}

void log_warn(struct log * _log, const char * format, ...)
{
	va_list ap;
	va_start(ap, format);
	log_printlevel(_log, LOG_WARNING, format, ap);
	va_end(ap);
}

void log_error(struct log * _log, const char * format, ...)
{
	va_list ap;
	va_start(ap, format);
	log_printlevel(_log, LOG_ERROR, format, ap);
	va_end(ap);
}

void log_critical(struct log * _log, const char * format, ...)
{
	va_list ap;
	va_start(ap, format);
	log_printlevel(_log, LOG_CRITICAL, format, ap);
	va_end(ap);
}

void log_assert(struct log * _log, int expression, const char * format, ...)
{
	if (expression != 1) {
		va_list ap;
		va_start(ap, format);
		log_printlevel(_log, LOG_CRITICAL, format, ap);
		va_end(ap);
		abort();
	}
}

