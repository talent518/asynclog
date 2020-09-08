#include "log_redis.h"

typedef struct _log_redis_t {
	struct _log_redis_t *next;
	size_t size;
	char data[1];
} log_redis_t;

log_status_t log_redis_init() {
	return SUCCESS;
}

log_status_t log_redis_write() {
	return SUCCESS;
}

log_status_t log_redis_push(const char *name, const char *level, const char *message, const zend_string *data, const char *category) {
	return SUCCESS;
}

log_status_t log_redis_request(const char *ctlname, const zend_string *request, const zend_string *globals, const char *content_type, zend_long content_length, int status, const zend_string *headers, const zend_string *output) {
	return SUCCESS;
}

log_status_t log_redis_destroy() {
	return SUCCESS;
}
