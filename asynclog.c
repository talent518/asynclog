#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "php.h"
#include "php_ini.h"
#include "SAPI.h"
#include "snprintf.h"
#include "zend_smart_str.h"
#include "standard/info.h"
#include "standard/php_var.h"
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

	if (buf.s) {
		smart_str_0(&buf);

		SYSLOG("name: %s, level: %ld, message: %s, data: %s, category: %s", name, level, message, buf.s->val, category);

		ret = strpprintf(0, "name: %s, level: %ld, message: %s, data: %s, category: %s", name, level, message, buf.s->val, category);
	} else {
		SYSLOG("name: %s, level: %ld, message: %s, data: %s, category: %s", name, level, message, "null", category);

		ret = strpprintf(0, "name: %s, level: %ld, message: %s, data: %s, category: %s", name, level, message, "null", category);
	}

	smart_str_free(&buf);

	RETURN_STR(ret);
}

PHP_GINIT_FUNCTION(asynclog) {
	OPENLOG();

	SYSLOG("GINIT");

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
	SYSLOG("GSHUTDOWN");

	CLOSELOG();
}

PHP_MINIT_FUNCTION(asynclog) {
	SYSLOG("MINIT");

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
	SYSLOG("MSHUTDOWN");

	UNREGISTER_INI_ENTRIES();

	return SUCCESS;
}

PHP_RINIT_FUNCTION(asynclog) {
#if defined(COMPILE_DL_ASYNCLOG) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	ASYNCLOG_G(reqtime) = microtime();

	SYSLOG("RINIT: %f\n", ASYNCLOG_G(reqtime));

	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(asynclog) {
	ASYNCLOG_G(restime) = microtime();

	smart_str buf = {0};

	{
		const int keys[] = {TRACK_VARS_POST, TRACK_VARS_GET, TRACK_VARS_COOKIE, TRACK_VARS_SERVER, TRACK_VARS_FILES};
		const char *vars[] = {"_POST", "_GET", "_COOKIE", "_SERVER", "_FILES"};
		zend_long i;

		for(i=0; i<sizeof(keys)/sizeof(const int); i++) {
			if(Z_TYPE(PG(http_globals)[keys[i]]) == IS_ARRAY && php_json_encode(&buf, &PG(http_globals)[keys[i]], PHP_JSON_PRETTY_PRINT) == SUCCESS && buf.s) {
				smart_str_0(&buf);
				SYSLOG("%s: %s", vars[i], ZSTR_VAL(buf.s));
				smart_str_free(&buf);
			}
		}
	}

	if (Z_TYPE(PG(http_globals)[TRACK_VARS_SERVER]) == IS_ARRAY) {
		HashTable *_SERVER = Z_ARRVAL(PG(http_globals)[TRACK_VARS_SERVER]);
		const char *ctlname;

		if(!SG(request_info).request_uri) {
			zval *prog, *argv, *argc, *arg0;
			zend_long i;

			prog = zend_hash_str_find(_SERVER, "_", sizeof("_")-1);
			argv = zend_hash_str_find(_SERVER, "argv", sizeof("argv")-1);
			argc = zend_hash_str_find(_SERVER, "argc", sizeof("argc")-1);

			if(Z_LVAL_P(argc) == 0) {
				smart_str_append_ex(&buf, Z_STR_P(prog), 0);
			} else {
				arg0 = zend_hash_index_find(Z_ARRVAL_P(argv), 0);

				if(strcmp(Z_STRVAL_P(prog), Z_STRVAL_P(arg0))) {
					smart_str_append_ex(&buf, Z_STR_P(prog), 0);
					smart_str_appendc_ex(&buf, ' ', 0);
				}

				smart_str_appendc_ex(&buf, '\'', 0);
				smart_str_append_ex(&buf, Z_STR_P(arg0), 0);
				smart_str_appendc_ex(&buf, '\'', 0);

				for(i=1; i<Z_LVAL_P(argc); i++) {
					zval *tmp = zend_hash_index_find(Z_ARRVAL_P(argv), i);
					smart_str_appendc_ex(&buf, ' ', 0);
					smart_str_appendc_ex(&buf, '\'', 0);
					smart_str_append_ex(&buf, Z_STR_P(tmp), 0);
					smart_str_appendc_ex(&buf, '\'', 0);
				}
			}

			ctlname = "COMMAND";
		} else {
			zval *method, *scheme, *host, *url, *user_agent;

			method = zend_hash_str_find(_SERVER, "REQUEST_METHOD", sizeof("REQUEST_METHOD")-1);
			scheme = zend_hash_str_find(_SERVER, "REQUEST_SCHEME", sizeof("REQUEST_SCHEME")-1);
			host = zend_hash_str_find(_SERVER, "HTTP_HOST", sizeof("HTTP_HOST")-1);
			url = zend_hash_str_find(_SERVER, "REQUEST_URI", sizeof("REQUEST_URI")-1);
			user_agent = zend_hash_str_find(_SERVER, "HTTP_USER_AGENT", sizeof("HTTP_USER_AGENT")-1);

			smart_str_append_ex(&buf, Z_STR_P(method), 0);
			smart_str_appendc_ex(&buf, ' ', 0);
			smart_str_append_ex(&buf, Z_STR_P(scheme), 0);
			smart_str_appendl_ex(&buf, "://", 3, 0);
			smart_str_append_ex(&buf, Z_STR_P(host), 0);
			smart_str_append_ex(&buf, Z_STR_P(url), 0);
			if(user_agent) {
				smart_str_appendl_ex(&buf, " \"", 2, 0);
				smart_str_append_ex(&buf, Z_STR_P(user_agent), 0);
				smart_str_appendc_ex(&buf, '"', 0);
			} else {
				smart_str_appendl_ex(&buf, " \"-\"", 4, 0);
			}

			ctlname = "REQUEST";
		}

		smart_str_0(&buf);
		SYSLOG("%s: %s", ctlname, ZSTR_VAL(buf.s));
		smart_str_free(&buf);

		if(SG(request_info).path_translated) {
			SYSLOG("PATH_TRANSLATED: %s", SG(request_info).path_translated);
		}
		if(SG(request_info).request_uri) {
			SYSLOG("REQUEST_URI: %s", SG(request_info).request_uri);
		}
		if(SG(request_info).content_length){
			SYSLOG("CONTENT_TYPE: %s", SG(request_info).content_type);
			SYSLOG("CONTENT_LENGTH: %ld", SG(request_info).content_length);
		}

		if(SG(request_info).request_body) {
			zend_string *post_data_str = NULL;

			php_stream_rewind(SG(request_info).request_body);
			post_data_str = php_stream_copy_to_mem(SG(request_info).request_body, PHP_STREAM_COPY_ALL, 0);
			if(post_data_str) {
				SYSLOG("POST_DATA: %s", ZSTR_VAL(post_data_str));
			}
		}
	} else {
		SYSLOG("REQUEST: -");
	}

	SYSLOG("RSHUTDOWN: %f - %.3fms\n", ASYNCLOG_G(restime), (ASYNCLOG_G(restime) - ASYNCLOG_G(reqtime)) * 1000);

	return SUCCESS;
}

PHP_MINFO_FUNCTION(asynclog) {
	SYSLOG("MINFO");

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
