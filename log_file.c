#include "php.h"
#include "php_ini.h"
#include "SAPI.h"
#include "snprintf.h"

#include "log_file.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>

typedef struct _log_file_t {
	struct _log_file_t *next;
	char name[128];
	size_t size;
	char data[32];
} log_file_t;

static log_file_t *head = NULL, *tail = NULL;
static char filepath[PATH_MAX];

log_status_t log_file_init() {
	SYSLOG("FILE_INIT");

	snprintf(filepath, sizeof(filepath), "%s", ASYNCLOG_G(filepath));

	return SUCCESS;
}

log_status_t log_file_write() {
	log_file_t *p = NULL;

	SYSLOG("FILE_WRITE1");
	LOG_POP(p);
	SYSLOG("FILE_WRITE2");

	if(p == NULL) return FAILURE;

	SYSLOG("FILE_WRITE3");

	char file[PATH_MAX];

	snprintf(file, sizeof(file), "%s%s.log", filepath, p->name);

	int fd = open(file, O_APPEND|O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

	if(fd > 0) {
		dprintf(fd, "[%s][%d]", sapi_module.name, getpid());
		write(fd, p->data, p->size);
		dprintf(fd, "\n==================================================================================================\n");

		close(fd);
	} else {
		SYSLOG("ERROR(%d): %s", errno, strerror(errno));
	}

	free(p);

	SYSLOG("FILE_WRITE4");

	return SUCCESS;
}

log_status_t log_file_begin_request() {
	return SUCCESS;
}

log_status_t log_file_push(const char *name, const char *category, const char *level, const char *message, const zend_string *data, double timestamp, double duration) {
	size_t size = snprintf(NULL, 0, "[%s][%s][%s] - %.3fms" "\n=== MESSAGE ===\n" "%s" "\n=== DATA ===\n" "%s", "2020-09-09 00:00:00", category, level, duration, message, data ? ZSTR_VAL(data) : "");
	log_file_t *p = (log_file_t*) malloc(sizeof(log_file_t) + size);
	char timestr[20] = "";
	struct tm *tm;
	time_t t = (time_t) timestamp;

	SYSLOG("FILE PUSH BEGIN");

	tm = localtime(&t);
	strftime(timestr, sizeof(timestr), "%F %T", tm);

	SYSLOG("NAME: %s-%d%02d%02d", name, tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday);

	snprintf(p->name, sizeof(p->name), "%s-%d%02d%02d", name, tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday);
	p->size = snprintf(p->data, size + sizeof(p->data), "[%s][%s][%s] - %.3fms" "\n=== MESSAGE ===\n" "%s" "\n=== DATA ===\n" "%s", timestr, category, level, duration, message, data ? ZSTR_VAL(data) : "");

	SYSLOG("FILE PUSH END: %lu %lu", size, p->size);

	LOG_PUSH(p);

	return SUCCESS;
}

log_status_t log_file_end_request(const char *ctlname, const zend_string *request, const zend_string *globals, const char *content_type, zend_long content_length, int status, const zend_string *headers, const zend_string *output, const zend_string *post_data_str) {
	size_t size = snprintf(NULL, 0, "[%s] %s - %.3fms - %s - %ld == %d" "\n=== HEADER ===" "%s" "\n=== OUTPUT ===\n" "%s" "\n=== GLOBALS ===\n" "%s" "\n=== REQUEST BODY ===\n" "%s", "2020-09-09 00:00:00", ZSTR_VAL(request), 0.00f, content_length > 0 && content_type ? content_type : "NONETYPE", content_length, status, headers ? ZSTR_VAL(headers) : "\n", output ? ZSTR_VAL(output) : "", globals ? ZSTR_VAL(globals) : "", post_data_str ? ZSTR_VAL(post_data_str) : "");
	log_file_t *p = (log_file_t*) malloc(sizeof(log_file_t) + size);
	char timestr[20] = "";
	struct tm *tm;
	double duration = (ASYNCLOG_G(restime) - ASYNCLOG_G(reqtime)) * 1000;
	time_t t = (time_t) ASYNCLOG_G(reqtime);

	SYSLOG("FILE REQ BEGIN");

	tm = localtime(&t);
	strftime(timestr, sizeof(timestr), "%F %T", tm);

	SYSLOG("CTLNAME: %s-%d%02d%02d", ctlname, tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday);

	snprintf(p->name, sizeof(p->name), "%s-%d%02d%02d", ctlname, tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday);
	p->size = snprintf(p->data, size + sizeof(p->data), "[%s] %s - %.3fms - %s %ld == %d" "\n=== HEADER ===" "%s" "\n=== OUTPUT ===\n" "%s" "\n=== GLOBALS ===\n" "%s" "\n=== REQUEST BODY ===\n" "%s", timestr, ZSTR_VAL(request), duration, content_length > 0 && content_type ? content_type : "NONETYPE", content_length, status, headers ? ZSTR_VAL(headers) : "\n", output ? ZSTR_VAL(output) : "", globals ? ZSTR_VAL(globals) : "", post_data_str ? ZSTR_VAL(post_data_str) : "");

	SYSLOG("FILE REQ END: %lu %lu", size, p->size);

	LOG_PUSH(p);

	return SUCCESS;
}

log_status_t log_file_destroy() {
	return SUCCESS;
}
