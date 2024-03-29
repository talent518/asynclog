#include "log.h"
#include "log_file.h"
#include "log_redis.h"
#include "log_elastic.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>

typedef struct _shm_status_t {
	sem_t sem;
	unsigned long total;
	unsigned long logs;
} shm_status_t;

typedef log_status_t (*handler_t)();
typedef log_status_t (*push_handler_t)(const char *name, const char *category, const char *level, const char *message, const zend_string *data, double timestamp, double duration);
typedef log_status_t (*end_request_t)(const char *ctlname, const zend_string *request, const zend_string *globals, const char *content_type, zend_long content_length, int status, const zend_string *headers, const zend_string *output, const zend_string *post_data_str);

static volatile int is_inited = 0;
static volatile int is_running = 0;
static sem_t msem; // max_logs for sem
static sem_t qsem; // queue for sem
static sem_t dsem; // destroy for sem
static sem_t lsem; // lock for sem
static handler_t write_handler = NULL, begin_request_handler = NULL, destroy_handler = NULL;
static push_handler_t push_handler = NULL;
static end_request_t end_request_handler = NULL;

static pthread_t thread;
static pthread_attr_t attr;
static shm_status_t *shm_status = NULL;

static void *log_write(void *arg) {
	int n = 0;
	SYSLOG("WORKER BEGIN");
	sem_post(&dsem);
	do {
		sem_wait(&qsem);
		SYSLOG("WORKER: %d", ++n);
	} while(write_handler() == SUCCESS);
	sem_post(&dsem);
	SYSLOG("WORKER END");

	pthread_detach(thread);
	pthread_exit(NULL);

	return NULL;
}

log_status_t log_init() {
	if(is_inited) {
		return is_inited < 0 ? FAILURE : SUCCESS;
	}
	is_inited++;
	if(is_inited > 1) {
		return SUCCESS;
	}

	int ret;
	switch(ASYNCLOG_G(type)) {
		case PHP_ASYNCLOG_MODE_FILE:
			write_handler = log_file_write;
			begin_request_handler = log_file_begin_request;
			push_handler = log_file_push;
			end_request_handler = log_file_end_request;
			destroy_handler = log_file_destroy;

			ret = log_file_init();
			break;
		case PHP_ASYNCLOG_MODE_REDIS:
			write_handler = log_redis_write;
			begin_request_handler = log_redis_begin_request;
			push_handler = log_redis_push;
			end_request_handler = log_redis_end_request;
			destroy_handler = log_redis_destroy;

			ret = log_redis_init();
			break;
		case PHP_ASYNCLOG_MODE_ELASTIC:
			write_handler = log_elastic_write;
			begin_request_handler = log_elastic_begin_request;
			push_handler = log_elastic_push;
			end_request_handler = log_elastic_end_request;
			destroy_handler = log_elastic_destroy;

			ret = log_elastic_init();
			break;
		default:
			is_inited = 0;
			return FAILURE;
	}

	if(ret != SUCCESS) {
		is_inited = -1;
		return ret;
	}

	SYSLOG("LOG_INIT");

	// init semaphore
	unsigned int value = ASYNCLOG_G(max_logs);
	if(value < 10) {
		value = 10;
	} else if(value > 1000000) {
		value = 1000000;
	}
	sem_init(&msem, 0, value);
	sem_init(&qsem, 0, 0);
	sem_init(&dsem, 0, 0);
	sem_init(&lsem, 0, 1);
	SYSLOG("MAX_LOGS: %u", value);

	{
		key_t key = ftok(ASYNCLOG_G(ftok_path), (int) ASYNCLOG_G(ftok_id));
		int id = shmget(key, sizeof(shm_status_t), IPC_CREAT | SHM_R | SHM_W);
		if(id < 0) {
			SYSLOG("SHMGET(%d): %s", errno, strerror(errno));
		} else {
			shm_status = shmat(id, NULL, 0);
			if(shm_status) sem_init(&shm_status->sem, 1, 1);
			else SYSLOG("SHMAT(%d): %s", errno, strerror(errno));
		}
	}

	// create thread
	pthread_attr_init(&attr);
	pthread_create(&thread, &attr, log_write, NULL);
	sem_wait(&dsem);
	SYSLOG("PTHREAD_CRAETED");

	return SUCCESS;
}

void log_lock() {
	sem_wait(&lsem);
}

void log_qpost() {
	sem_post(&qsem);
}

void log_unlock() {
	sem_post(&lsem);
}

void log_mwait() {
	sem_wait(&msem);
}

void log_mpost() {
	sem_post(&msem);
}

void log_swait() {
	if(shm_status) {
		unsigned long total, logs;

		sem_wait(&shm_status->sem);
		total = shm_status->total;
		logs = -- shm_status->logs;
		sem_post(&shm_status->sem);

		SYSLOG("SHM DECR: total(%lu), logs(%lu), micromtime(%lf)", total, logs, microtime());
	}
}

void log_spost() {
	if(shm_status) {
		unsigned long total, logs;

		sem_wait(&shm_status->sem);
		total = ++ shm_status->total;
		logs = ++ shm_status->logs;
		sem_post(&shm_status->sem);

		SYSLOG("SHM INCR: total(%lu), logs(%lu), microtime(%lf)", total, logs, microtime());
	}
}

log_status_t log_begin_request() {
	return begin_request_handler();
}

log_status_t log_push(const char *name, const char *category, const char *level, const char *message, const zend_string *data, double timestamp, double duration) {
	if((is_inited == 0 && log_init() == FAILURE) || is_inited < 0) return FAILURE;

	return push_handler(name, category, level, message, data, timestamp, duration);
}

log_status_t log_end_request(const char *ctlname, const zend_string *request, const zend_string *globals, const char *content_type, zend_long content_length, int status, const zend_string *headers, const zend_string *output, const zend_string *post_data_str) {
	if((is_inited == 0 && log_init() == FAILURE) || is_inited < 0) return FAILURE;

	return end_request_handler(ctlname, request, globals, content_type, content_length, status, headers, output, post_data_str);
}

void log_destroy() {
	if(is_inited <= 0) {
		return;
	}

	SYSLOG("LOG_DESTROY");

	is_running = 0;
	sem_post(&qsem);

	sem_wait(&dsem);

	SYSLOG("LOG_DESTROY 1");

	sem_destroy(&msem);
	sem_destroy(&qsem);
	sem_destroy(&dsem);
	sem_destroy(&lsem);

	SYSLOG("LOG_DESTROY 2");

	if(shm_status) {
		if(shmdt(shm_status) < 0) {
			SYSLOG("SHMDT(%d): %s", errno, strerror(errno));
		}
		sem_destroy(&shm_status->sem);
	}

	SYSLOG("LOG_DESTROY 3");

	is_inited = 0;

	if(destroy_handler() == FAILURE) {
		SYSLOG("LOG_DESTROY failure");
	}

	exit(EG(exit_status));
}
