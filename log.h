#ifndef _LOG_H
#define _LOG_H

#include "zend_types.h"
#include "php_asynclog.h"

#define log_status_t ZEND_RESULT_CODE

log_status_t log_init();
log_status_t log_write();
log_status_t log_begin_request();
log_status_t log_push(const char *name, const char *level, const char *message, const zend_string *data, const char *category, double runTime);
log_status_t log_end_request(const char *ctlname, const zend_string *request, const zend_string *globals, const char *content_type, zend_long content_length, int status, const zend_string *headers, const zend_string *output);
log_status_t log_destroy();

#endif /* _LOG_H */
