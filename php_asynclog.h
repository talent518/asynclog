#ifndef PHP_ASYNCLOG_H
#define PHP_ASYNCLOG_H

#include "php.h"

extern zend_module_entry asynclog_module_entry;
#define phpext_asynclog_ptr &asynclog_module_entry

#define PHP_ASYNCLOG_VERSION        "0.1.0"

#define PHP_ASYNCLOG_MODE_FILE      (1<<0)
#define PHP_ASYNCLOG_MODE_REDIS     (1<<1)
#define PHP_ASYNCLOG_MODE_ELASTIC   (1<<2)

#define PHP_ASYNCLOG_LEVEL_ERROR    (1<<0)
#define PHP_ASYNCLOG_LEVEL_WARN     (1<<1)
#define PHP_ASYNCLOG_LEVEL_INFO     (1<<2)
#define PHP_ASYNCLOG_LEVEL_DEBUG    (1<<3)
#define PHP_ASYNCLOG_LEVEL_VERBOSE  (1<<4)
#define PHP_ASYNCLOG_LEVEL_ALL (PHP_ASYNCLOG_LEVEL_ERROR | PHP_ASYNCLOG_LEVEL_WARN | PHP_ASYNCLOG_LEVEL_INFO | PHP_ASYNCLOG_LEVEL_DEBUG | PHP_ASYNCLOG_LEVEL_VERBOSE)

#ifdef PHP_WIN32
#	define PHP_ASYNCLOG_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_ASYNCLOG_API __attribute__ ((visibility("default")))
#else
#	define PHP_ASYNCLOG_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#ifdef ASYNCLOG_DEBUG
#	include <syslog.h>
#	include "SAPI.h"
#	define OPENLOG()            openlog("asynclog", LOG_PID | LOG_CONS, LOG_USER)
#	define SYSLOG(fmt, args...) syslog(LOG_DEBUG, "%s -> " fmt, sapi_module.name, ##args)
#	define CLOSELOG()           closelog()
#	define INILOG(fmt)          SYSLOG(#fmt " CONFIG: threads: %ld, type: %ld, level: %ld, filepath: %s, redis_host: %s, redis_port: %ld, redis_auth: %s, elastic: %s, category: %s", ASYNCLOG_G(threads), ASYNCLOG_G(type), ASYNCLOG_G(level), ASYNCLOG_G(filepath), ASYNCLOG_G(redis_host), ASYNCLOG_G(redis_port), ASYNCLOG_G(redis_auth), ASYNCLOG_G(elastic), ASYNCLOG_G(category))
#else
#	define OPENLOG()            ((void)0)
#	define SYSLOG(fmt, args...) ((void)0)
#	define CLOSELOG()           ((void)0)
#	define INILOG(fmt)             ((void)0)
#endif

ZEND_BEGIN_MODULE_GLOBALS(asynclog)
	// run time
	double     reqtime;
	double     itertime;
	double     restime;

	// php.ini config
	zend_long  threads;
	zend_long  type;
	zend_long  level;
	zend_long  redis_port;
	char *filepath;
	char *redis_host;
	char *redis_auth;
	char *elastic;
	char *category;
	zend_long max_output;
	zend_long max_logs;

	// response buffer
	smart_str output;
	zend_long output_len;

	char is_shutdown; // The php-fpm worker process pool is stoping or reloading
ZEND_END_MODULE_GLOBALS(asynclog)

ZEND_EXTERN_MODULE_GLOBALS(asynclog);

#define ASYNCLOG_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(asynclog, v)

#if defined(ZTS) && defined(COMPILE_DL_ASYNCLOG)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#endif	/* PHP_ASYNCLOG_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
