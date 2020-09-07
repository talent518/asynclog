#ifndef _LOG_H
#define _LOG_H

#include "zend_types.h"
#include "php_asynclog.h"

#define log_status_t ZEND_RESULT_CODE

log_status_t log_init();
log_status_t log_push(const char *name, int level, const char *message, const zval *data, const char *category);
log_status_t log_request(const char *url, const char *method, double reqtime, double runtime, const zval *globals, const char *userAgent, const char *contentType, const char *contentLength);
log_status_t log_destroy();

#endif /* _LOG_H */
