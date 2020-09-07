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

log_status_t log_file_push(const char *name, int level, const char *message, const zval *data, const char *category) {
	return SUCCESS;
}

log_status_t log_file_request(const char *url, const char *method, double reqtime, double runtime, const zval *globals, const char *userAgent, const char *contentType, const char *contentLength) {
	return SUCCESS;
}

log_status_t log_file_destroy() {
	return SUCCESS;
}
