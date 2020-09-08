#ifndef _LOG_ELASTIC_H
#define _LOG_ELASTIC_H

#include "log.h"

log_status_t log_elastic_init();
log_status_t log_elastic_write();
log_status_t log_elastic_begin_request();
log_status_t log_elastic_push(const char *name, const char *level, const char *message, const zend_string *data, const char *category, double runTime);
log_status_t log_elastic_end_request(const char *ctlname, const zend_string *request, const zend_string *globals, const char *content_type, zend_long content_length, int status, const zend_string *headers, const zend_string *output);
log_status_t log_elastic_destroy();

#endif /* _LOG_ELASTIC_H */
