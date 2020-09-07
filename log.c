#include "log.h"
#include "log_file.h"
#include "log_redis.h"
#include "log_elastic.h"

log_status_t log_init() {
	return SUCCESS;
}

log_status_t log_push(const char *name, int level, const char *message, const zval *data, const char *category) {
	return SUCCESS;
}

log_status_t log_request(const char *url, const char *method, double reqtime, double runtime, const zval *globals, const char *userAgent, const char *contentType, const char *contentLength) {
	return SUCCESS;
}

log_status_t log_destroy() {
	return SUCCESS;
}
