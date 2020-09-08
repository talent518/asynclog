#include "log.h"
#include "log_file.h"
#include "log_redis.h"
#include "log_elastic.h"

log_status_t log_init() {
	return SUCCESS;
}

log_status_t log_write() {
	return SUCCESS;
}

log_status_t log_begin_request() {
	return SUCCESS;
}

log_status_t log_push(const char *name, const char *level, const char *message, const zend_string *data, const char *category, double runTime) {
	return SUCCESS;
}

log_status_t log_end_request(const char *ctlname, const zend_string *request, const zend_string *globals, const char *content_type, zend_long content_length, int status, const zend_string *headers, const zend_string *output) {
	return SUCCESS;
}

log_status_t log_destroy() {
	return SUCCESS;
}
