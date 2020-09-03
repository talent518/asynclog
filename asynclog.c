/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2018 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "SAPI.h"
#include "zend_smart_str.h"
#include "standard/info.h"
#include "json/php_json.h"

#include "api.h"
#include "php_asynclog.h"

ZEND_DECLARE_MODULE_GLOBALS(asynclog);

PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("asynclog.threads",                        "1", PHP_INI_ALL, OnUpdateLong,   threads,    zend_asynclog_globals, asynclog_globals)
    STD_PHP_INI_ENTRY("asynclog.type",          "ASYNCLOG_LEVEL_ALL", PHP_INI_ALL, OnUpdateLong,   type,       zend_asynclog_globals, asynclog_globals)
    STD_PHP_INI_ENTRY("asynclog.filepath",      "/var/log/asynclog/", PHP_INI_ALL, OnUpdateString, filepath,   zend_asynclog_globals, asynclog_globals)
    STD_PHP_INI_ENTRY("asynclog.redis_host",             "127.0.0.1", PHP_INI_ALL, OnUpdateString, redis_host, zend_asynclog_globals, asynclog_globals)
    STD_PHP_INI_ENTRY("asynclog.redis_port",                  "6379", PHP_INI_ALL, OnUpdateLong,   redis_port, zend_asynclog_globals, asynclog_globals)
    STD_PHP_INI_ENTRY("asynclog.redis_auth",                      "", PHP_INI_ALL, OnUpdateString, redis_auth, zend_asynclog_globals, asynclog_globals)
    STD_PHP_INI_ENTRY("asynclog.elastic",    "http://127.0.0.1:9200", PHP_INI_ALL, OnUpdateString, redis_auth, zend_asynclog_globals, asynclog_globals)
    STD_PHP_INI_ENTRY("asynclog.category",             "application", PHP_INI_ALL, OnUpdateString, category,   zend_asynclog_globals, asynclog_globals)
PHP_INI_END()

ZEND_BEGIN_ARG_INFO_EX(arginfo_asynclog, 0, 0, 3)
	ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, level, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, message, IS_STRING, 0)
	ZEND_ARG_INFO(0, data)
	ZEND_ARG_TYPE_INFO(0, category, IS_STRING, 0)
ZEND_END_ARG_INFO()

PHP_FUNCTION(asynclog) {
	char *name = NULL;
	size_t name_len;

	zend_long level = 0;

	char *message = NULL;
	size_t message_len;

	zval *data = NULL;

	char *category = NULL;
	size_t category_len;

	smart_str buf = {0};

	zend_string *ret;

	if (zend_parse_parameters_throw(ZEND_NUM_ARGS(), "sls|as", &name, &name_len, &level, &message, &message_len, &data, &category, &category_len) == FAILURE) {
		return;
	}

	if(data && php_json_encode(&buf, data, PHP_JSON_PRETTY_PRINT) == FAILURE) {
		switch(JSON_G(error_code)) {
			case PHP_JSON_ERROR_NONE:
				RETURN_STRING("No error");
			case PHP_JSON_ERROR_DEPTH:
				RETURN_STRING("Maximum stack depth exceeded");
			case PHP_JSON_ERROR_STATE_MISMATCH:
				RETURN_STRING("State mismatch (invalid or malformed JSON)");
			case PHP_JSON_ERROR_CTRL_CHAR:
				RETURN_STRING("Control character error, possibly incorrectly encoded");
			case PHP_JSON_ERROR_SYNTAX:
				RETURN_STRING("Syntax error");
			case PHP_JSON_ERROR_UTF8:
				RETURN_STRING("Malformed UTF-8 characters, possibly incorrectly encoded");
			case PHP_JSON_ERROR_RECURSION:
				RETURN_STRING("Recursion detected");
			case PHP_JSON_ERROR_INF_OR_NAN:
				RETURN_STRING("Inf and NaN cannot be JSON encoded");
			case PHP_JSON_ERROR_UNSUPPORTED_TYPE:
				RETURN_STRING("Type is not supported");
			case PHP_JSON_ERROR_INVALID_PROPERTY_NAME:
				RETURN_STRING("The decoded property name is invalid");
			case PHP_JSON_ERROR_UTF16:
				RETURN_STRING("Single unpaired UTF-16 surrogate in unicode escape");
			default:
				RETURN_STRING("Unknown error");
		}
	}
	if(!category) {
		category = "application";
	}

	smart_str_0(&buf); /* copy? */
	if (buf.s) {
		ret = strpprintf(0, "name: %s, level: %d, message: %s, data: %s, category: %s", name, level, message, buf.s->val, category);
	} else {
		ret = strpprintf(0, "name: %s, level: %d, message: %s, data: %s, category: %s", name, level, message, "null", category);
	}

	smart_str_free(&buf);

	RETURN_STR(ret);
}

PHP_GINIT_FUNCTION(asynclog) {
	dprintf("GINIT: asynclog\n");

	asynclog_globals->reqtime    = 0;
	asynclog_globals->restime    = 0;

	asynclog_globals->threads    = 1;
	asynclog_globals->type       = 1;
	asynclog_globals->filepath   = NULL;
	asynclog_globals->redis_host = NULL;
	asynclog_globals->redis_port = 1;
	asynclog_globals->redis_auth = NULL;
	asynclog_globals->elastic    = NULL;
}

PHP_GSHUTDOWN_FUNCTION(asynclog) {
	dprintf("GSHUTDOWN: asynclog\n");
}

PHP_MINIT_FUNCTION(asynclog) {
	dprintf("MINIT: asynclog\n");

	REGISTER_INI_ENTRIES();

	REGISTER_STRING_CONSTANT("ASYNCLOG_VERSION",     PHP_ASYNCLOG_VERSION,       CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("ASYNCLOG_LEVEL_ERROR",   PHP_ASYNCLOG_LEVEL_ERROR,   CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("ASYNCLOG_LEVEL_WARN",    PHP_ASYNCLOG_LEVEL_WARN,    CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("ASYNCLOG_LEVEL_INFO",    PHP_ASYNCLOG_LEVEL_INFO,    CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("ASYNCLOG_LEVEL_DEBUG",   PHP_ASYNCLOG_LEVEL_DEBUG,   CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("ASYNCLOG_LEVEL_VERBOSE", PHP_ASYNCLOG_LEVEL_VERBOSE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("ASYNCLOG_LEVEL_ALL",     PHP_ASYNCLOG_LEVEL_VERBOSE, CONST_CS | CONST_PERSISTENT);

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(asynclog) {
	dprintf("MSHUTDOWN: asynclog\n");

	UNREGISTER_INI_ENTRIES();

	return SUCCESS;
}

PHP_RINIT_FUNCTION(asynclog) {
	ASYNCLOG_G(reqtime) = microtime();

	dprintf("RINIT: asynclog - %f\n", ASYNCLOG_G(reqtime));
	hprintf("<hr/>");

#if defined(COMPILE_DL_ASYNCLOG) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(asynclog) {
	ASYNCLOG_G(restime) = microtime();

	hprintf("<hr/>");
	dprintf("RSHUTDOWN: asynclog - %f - %.3fms\n", ASYNCLOG_G(restime), (ASYNCLOG_G(restime) - ASYNCLOG_G(reqtime)) * 1000);

	return SUCCESS;
}

PHP_MINFO_FUNCTION(asynclog) {
	php_info_print_table_start();
	php_info_print_table_header(2, "asynclog support", "enabled");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}

const zend_function_entry asynclog_functions[] = {
	PHP_FE(asynclog, arginfo_asynclog)
	PHP_FE_END
};

zend_module_entry asynclog_module_entry = {
	STANDARD_MODULE_HEADER,
	"asynclog",
	asynclog_functions,
	PHP_MINIT(asynclog),
	PHP_MSHUTDOWN(asynclog),
	PHP_RINIT(asynclog),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(asynclog),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(asynclog),
	PHP_ASYNCLOG_VERSION,
	PHP_MODULE_GLOBALS(asynclog),
	PHP_GINIT(asynclog),
	PHP_GSHUTDOWN(asynclog),
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_ASYNCLOG
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(asynclog)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
