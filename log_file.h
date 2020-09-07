#ifndef _LOG_FILE_H
#define _LOG_FILE_H

#include "log.h"

log_status_t log_file_init();
log_status_t log_file_write();
log_status_t log_file_push(const char *name, int level, const char *message, const zval *data, const char *category);
log_status_t log_file_request(const char *url, const char *method, double reqtime, double runtime, const zval *globals, const char *userAgent, const char *contentType, const char *contentLength);
log_status_t log_file_destroy();

#endif /* _LOG_FILE_H */
