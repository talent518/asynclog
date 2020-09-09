#include "php.h"
#include "php_ini.h"
#include "SAPI.h"
#include "snprintf.h"

#include "log_file.h"

#include <stdio.h>
#include <string.h>

typedef struct _log_file_t {
	struct _log_file_t *next;
	char name[51];
	size_t size;
	char data[1];
} log_file_t;

static log_file_t *head = NULL, *tail = NULL;

log_status_t log_file_init() {
	SYSLOG("FILE_INIT");
	return SUCCESS;
}

log_status_t log_file_write() {
	log_file_t *p = NULL;

	SYSLOG("FILE_WRITE1");
	LOG_POP(p);
	SYSLOG("FILE_WRITE2");

	if(p == NULL) return FAILURE;

	SYSLOG("FILE_WRITE3");

	free(p);

	SYSLOG("FILE_WRITE4");

	return SUCCESS;
}

log_status_t log_file_begin_request() {
	return SUCCESS;
}

log_status_t log_file_push(const char *name, const char *category, const char *level, const char *message, const zend_string *data, double timestamp, double duration) {
	size_t size = snprintf(NULL, 0, "[%s][%s][%s] - %.3fms === %s >>> %s", "2020-09-09 00:00:00", category, level, duration, message, data ? ZSTR_VAL(data) : "");
	log_file_t *p = (log_file_t*) malloc(sizeof(log_file_t) + size);
	char timestr[20] = "";
	struct tm *tm;
	time_t t = (time_t) timestamp;

	SYSLOG("FILE PUSH BEGIN");

	tm = localtime(&t);
	strftime(timestr, sizeof(timestr), "%F %T", tm);

	snprintf(p->name, sizeof(p->name), "%s", name);
	p->size = snprintf(p->data, size, "[%s][%s][%s] - %.3fms === %s >>> %s", timestr, category, level, duration, message, data ? ZSTR_VAL(data) : "");

	SYSLOG("FILE PUSH END: %lu %lu", size, p->size);

	LOG_PUSH(p);

	return SUCCESS;
}

log_status_t log_file_end_request(const char *ctlname, const zend_string *request, const zend_string *globals, const char *content_type, zend_long content_length, int status, const zend_string *headers, const zend_string *output) {
	log_file_t *p = (log_file_t*) malloc(sizeof(log_file_t));
	LOG_PUSH(p);
	return SUCCESS;
}

log_status_t log_file_destroy() {
	return SUCCESS;
}
