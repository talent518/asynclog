#include "log_elastic.h"

typedef struct _log_elastic_t {
	struct _log_elastic_t *next;
	size_t size;
	char data[1];
} log_elastic_t;

log_status_t log_elastic_init() {
	return SUCCESS;
}

log_status_t log_elastic_write() {
	return SUCCESS;
}

log_status_t log_elastic_begin_request() {
	return SUCCESS;
}

log_status_t log_elastic_push(const char *name, const char *category, const char *level, const char *message, const zend_string *data, double timestamp, double duration) {
	return SUCCESS;
}

log_status_t log_elastic_end_request(const char *ctlname, const zend_string *request, const zend_string *globals, const char *content_type, zend_long content_length, int status, const zend_string *headers, const zend_string *output, const zend_string *post_data_str) {
	return SUCCESS;
}

log_status_t log_elastic_destroy() {
	return SUCCESS;
}
