#include "php.h"
#include "php_ini.h"
#include "SAPI.h"
#include "snprintf.h"

#include "log_file.h"

typedef struct _log_file_t {
	struct _log_file_t *next;
	size_t size;
	char data[1];
} log_file_t;


log_status_t log_file_init() {
	return SUCCESS;
}

log_status_t log_file_write() {
	return SUCCESS;
}

log_status_t log_file_begin_request() {
	return SUCCESS;
}

log_status_t log_file_push(const char *name, const char *level, const char *message, const zend_string *data, const char *category, double runTime) {
	return SUCCESS;
}

log_status_t log_file_end_request(const char *ctlname, const zend_string *request, const zend_string *globals, const char *content_type, zend_long content_length, int status, const zend_string *headers, const zend_string *output) {
	return SUCCESS;
}

log_status_t log_file_destroy() {
	return SUCCESS;
}
