#ifndef _LOG_H
#define _LOG_H

#include "zend_types.h"
#include "php_asynclog.h"

#define log_status_t ZEND_RESULT_CODE

log_status_t log_init();
log_status_t log_begin_request();
log_status_t log_push(const char *name, const char *category, const char *level, const char *message, const zend_string *data, double timestamp, double duration);
log_status_t log_end_request(const char *ctlname, const zend_string *request, const zend_string *globals, const char *content_type, zend_long content_length, int status, const zend_string *headers, const zend_string *output);
void log_destroy();

void log_lock();
void log_qpost();
void log_mwait();
void log_mpost();
void log_unlock();

#define LOG_PUSH(p) \
	do { \
		p->next = NULL; \
		log_lock(); \
		if(head == NULL) { \
			head = tail = p; \
		} else { \
			tail->next = p; \
			tail = p; \
		} \
		log_unlock(); \
		log_qpost(); \
		log_mwait(); \
	} while (0)

#define LOG_POP(p) \
	do { \
		log_lock(); \
		if(head) { \
			p = head; \
			head = head->next; \
			if(p == tail) { \
				tail = NULL; \
			} \
		} \
		log_unlock(); \
		log_mpost(); \
	} while (0)

#endif /* _LOG_H */
