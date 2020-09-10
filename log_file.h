#ifndef _LOG_FILE_H
#define _LOG_FILE_H

#include "log.h"

log_status_t log_file_init();
log_status_t log_file_write();
log_status_t log_file_begin_request();
log_status_t log_file_push(const char *name, const char *category, const char *level, const char *message, const zend_string *data, double timestamp, double duration);
log_status_t log_file_end_request(const char *ctlname, const zend_string *request, const zend_string *globals, const char *content_type, zend_long content_length, int status, const zend_string *headers, const zend_string *output, const zend_string *post_data_str);
log_status_t log_file_destroy();

#endif /* _LOG_FILE_H */
