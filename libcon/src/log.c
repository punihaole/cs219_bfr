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

	if (mode == LOG_PRESERVE) {
		FILE *fp = fopen(filename, "r");
		if (fp) {
			fclose(fp);
			return -1;
		}
	}

	if (mode == LOG_APPEND) _log->log_fp = fopen(filename, "a");
	if (mode == LOG_OVERWRITE) _log->log_fp = fopen(filename, "w");

	if (_log->log_fp == NULL) return -1;

	_log->log_name = (char * ) malloc(strlen(log_name));
	_log->pending_writes = 0;
	strcpy(_log->log_name, log_name);

	return 0;
}

int log_close(struct log * _log)
{
	if (!_log) return -1;

	fclose(_log->log_fp);
	free(_log->log_name);
	free(_log);
	_log = NULL;

	return 0;
}

void log_print(struct log * _log, const char * format, ...)
{
	if (!_log || !format) return;

	struct tm * current;
	time_t now;

	time(&now);
	current = localtime(&now);

	char * date = asctime(current);
	date[strlen(date)-1] = '\0';
	fprintf(_log->log_fp, "%s %s: ", date, _log->log_name);

	va_list ap;
	va_start(ap, format);
	vfprintf(_log->log_fp, format, ap);
	fprintf(_log->log_fp, "\n");
	_log->pending_writes++;

	if (_log->pending_writes > MAX_PENDING_WRITES) {
		fflush(_log->log_fp);
		_log->pending_writes = 0;
	}
	va_end(ap);
}

void log_printnow(struct log * _log, const char * format, ...)
{
	if (!_log || !format) return;

	struct tm * current;
	time_t now;

	time(&now);
	current = localtime(&now);

	char * date = asctime(current);
	date[strlen(date)-1] = '\0';
	fprintf(_log->log_fp, "%s %s: ", date, _log->log_name);

	va_list ap;
	va_start(ap, format);
	vfprintf(_log->log_fp, format, ap);
	fprintf(_log->log_fp, "\n");
	fflush(_log->log_fp);
	_log->pending_writes = 0;
	va_end(ap);
}
