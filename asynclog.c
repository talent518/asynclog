#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include "php.h"
#include "php_ini.h"
#include "SAPI.h"
#include "snprintf.h"
#include "zend_smart_str.h"
#include "standard/info.h"
#include "standard/php_var.h"
#include "json/php_json.h"
#include "fastcgi.h"
#include "php_variables.h"

#include "api.h"
#include "php_asynclog.h"
#include "log.h"

ZEND_DECLARE_MODULE_GLOBALS(asynclog);

PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("asynclog.type",                           "1", PHP_INI_SYSTEM, OnUpdateLong,   type,       zend_asynclog_globals, asynclog_globals)
    STD_PHP_INI_ENTRY("asynclog.level",                         "31", PHP_INI_SYSTEM, OnUpdateLong,   level,      zend_asynclog_globals, asynclog_globals)
    STD_PHP_INI_ENTRY("asynclog.filepath",      "/var/log/asynclog/", PHP_INI_SYSTEM, OnUpdateString, filepath,   zend_asynclog_globals, asynclog_globals)
    STD_PHP_INI_ENTRY("asynclog.redis_host",             "127.0.0.1", PHP_INI_SYSTEM, OnUpdateString, redis_host, zend_asynclog_globals, asynclog_globals)
    STD_PHP_INI_ENTRY("asynclog.redis_port",                  "6379", PHP_INI_SYSTEM, OnUpdateLong,   redis_port, zend_asynclog_globals, asynclog_globals)
    STD_PHP_INI_ENTRY("asynclog.redis_auth",                      "", PHP_INI_SYSTEM, OnUpdateString, redis_auth, zend_asynclog_globals, asynclog_globals)
    STD_PHP_INI_ENTRY("asynclog.elastic",    "http://127.0.0.1:9200", PHP_INI_SYSTEM, OnUpdateString, elastic,    zend_asynclog_globals, asynclog_globals)
    STD_PHP_INI_ENTRY("asynclog.max_output",               "1048576", PHP_INI_SYSTEM, OnUpdateString, max_output, zend_asynclog_globals, asynclog_globals)
    STD_PHP_INI_ENTRY("asynclog.max_logs",                   "10000", PHP_INI_SYSTEM, OnUpdateString, max_logs, zend_asynclog_globals, asynclog_globals)
    STD_PHP_INI_ENTRY("asynclog.ftok_path",      "/var/log/asynclog", PHP_INI_SYSTEM, OnUpdateString, ftok_path, zend_asynclog_globals, asynclog_globals)
    STD_PHP_INI_ENTRY("asynclog.ftok_id",                       "87", PHP_INI_SYSTEM, OnUpdateLong,   ftok_id, zend_asynclog_globals, asynclog_globals)
PHP_INI_END()

ZEND_BEGIN_ARG_INFO_EX(arginfo_asynclog, 0, 0, 5)
	ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, category, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, level, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, message, IS_STRING, 0)
	ZEND_ARG_INFO(0, data)
	ZEND_ARG_TYPE_INFO(0, timestamp, IS_DOUBLE, 0)
	ZEND_ARG_TYPE_INFO(0, duration, IS_DOUBLE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_asynclog_shutdown, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, source, IS_STRING, 0)
ZEND_END_ARG_INFO()

static void php_head_apply_header_list_to_hash(void *data, void *arg) {
	sapi_header_struct *sapi_header = (sapi_header_struct *)data;
	smart_str *buf = (smart_str *) arg;

	if (buf && sapi_header) {
		smart_str_appendl(buf, PHP_EOL, sizeof(PHP_EOL)-1);
		smart_str_appendl(buf, sapi_header->header, sapi_header->header_len);
	}
}

static void php_asynclog_output_handler(char *output, size_t output_len, char **handled_output, size_t *handled_output_len, int mode) {
	if(!output || !output_len) {
		return;
	}

	ASYNCLOG_G(output_len) += output_len;
	if(ASYNCLOG_G(max_output) <= 0 || smart_str_get_len(&ASYNCLOG_G(output)) + output_len < ASYNCLOG_G(max_output)) {
		smart_str_appendl(&ASYNCLOG_G(output), output, output_len);
	}
}

static void sig_handler(int sig) {
	SYSLOG("SIG: %d", sig);

	ASYNCLOG_G(is_shutdown) = 1;
}

#define JSON_POST_CONTENT_TYPE "application/json"

static SAPI_POST_READER_FUNC(json_post_reader) {
	SYSLOG("POST_READER");
	if (!SG(request_info).request_body) {
		sapi_read_standard_form_data();
	}
}

static SAPI_POST_HANDLER_FUNC(json_post_handler) {
	if(SG(request_info).request_body) {
		php_stream_rewind(SG(request_info).request_body);
		const zend_string *post_data_str = php_stream_copy_to_mem(SG(request_info).request_body, PHP_STREAM_COPY_ALL, 0);
		if(post_data_str) {
			SYSLOG("POST_HANDLER: %s", ZSTR_VAL(post_data_str));
			zval_ptr_dtor_nogc(&PG(http_globals)[TRACK_VARS_POST]);
			php_json_decode_ex(&PG(http_globals)[TRACK_VARS_POST], ZSTR_VAL(post_data_str), ZSTR_LEN(post_data_str), PHP_JSON_OBJECT_AS_ARRAY|PHP_JSON_THROW_ON_ERROR, PHP_JSON_PARSER_DEFAULT_DEPTH);
			zend_string_free(post_data_str);
		} else {
			SYSLOG("POST_HANDLER");
		}
	} else {
		SYSLOG("POST_HANDLER");
	}
}

static const sapi_post_entry php_post_entries[] = {
    { JSON_POST_CONTENT_TYPE, sizeof(JSON_POST_CONTENT_TYPE)-1, json_post_reader, json_post_handler },
    { NULL, 0, NULL, NULL }
};

static void sapi_read_post_data(void) {
    sapi_post_entry *post_entry;
    uint32_t content_type_length = (uint32_t)strlen(SG(request_info).content_type);
    char *content_type = estrndup(SG(request_info).content_type, content_type_length);
    char *p;
    char oldchar=0;
    void (*post_reader_func)(void) = NULL;


    /* dedicated implementation for increased performance:
     * - Make the content type lowercase
     * - Trim descriptive data, stay with the content-type only
     */
    for (p=content_type; p<content_type+content_type_length; p++) {
        switch (*p) {
            case ';':
            case ',':
            case ' ':
                content_type_length = p-content_type;
                oldchar = *p;
                *p = 0;
                break;
            default:
                *p = tolower(*p);
                break;
        }
    }

    /* now try to find an appropriate POST content handler */
    if ((post_entry = zend_hash_str_find_ptr(&SG(known_post_content_types), content_type,
            content_type_length)) != NULL) {
        /* found one, register it for use */
        SG(request_info).post_entry = post_entry;
        post_reader_func = post_entry->post_reader;
    } else {
        /* fallback */
        SG(request_info).post_entry = NULL;
        if (!sapi_module.default_post_reader) {
            /* no default reader ? */
            SG(request_info).content_type_dup = NULL;
            sapi_module.sapi_error(E_WARNING, "Unsupported content type:  '%s'", content_type);
            return;
        }
    }
    if (oldchar) {
        *(p-1) = oldchar;
    }

    SG(request_info).content_type_dup = content_type;

    if(post_reader_func) {
        post_reader_func();
    }

    if(sapi_module.default_post_reader) {
        sapi_module.default_post_reader();
    }
}

static void (*old_treat_data)(int arg, char *str, zval *destArray);

static SAPI_TREAT_DATA_FUNC(restful_treat_data) {
	SYSLOG("POST_TREAT: %d, %s, %p", arg, str, destArray);

	if(arg == PARSE_POST && !SG(request_info).request_body && SG(request_info).content_length) {
		sapi_read_post_data();
	}

	old_treat_data(arg, str, destArray);
}

static zend_bool php_auto_globals_create_post(zend_string *name)
{
	SYSLOG("CREATE_POST1");

    if (SG(request_info).content_length && !SG(headers_sent)) {
    	SYSLOG("CREATE_POST2");
        sapi_module.treat_data(PARSE_POST, NULL, &PG(http_globals)[TRACK_VARS_POST]);
    } else {
    	SYSLOG("CREATE_POST3");
        zval_ptr_dtor_nogc(&PG(http_globals)[TRACK_VARS_POST]);
        array_init(&PG(http_globals)[TRACK_VARS_POST]);
    }
	SYSLOG("CREATE_POST4");

    zend_hash_update(&EG(symbol_table), name, &PG(http_globals)[TRACK_VARS_POST]);
    Z_ADDREF(PG(http_globals)[TRACK_VARS_POST]);
	SYSLOG("CREATE_POST5");

    return 0; /* don't rearm */
}


PHP_FUNCTION(asynclog) {
	const char *name = NULL;
	size_t name_len;

	zend_long level = 0;
	const char *levelstr = NULL;

	const char *message = NULL;
	size_t message_len;

	zval *data = NULL;

	const char *category = NULL;
	size_t category_len;

	smart_str buf = {0};

	zend_string *ret;

	double itertime = microtime(), t;
	double timestamp = itertime;
	double duration = -1;

	if (zend_parse_parameters_throw(ZEND_NUM_ARGS(), "sslsa|dd", &name, &name_len, &category, &category_len, &level, &message, &message_len, &data, &timestamp, &duration) == FAILURE) {
		return;
	}

	// INILOG(FUNC);

	if((ASYNCLOG_G(level) & level) != level) {
		RETURN_FALSE;
	}

	switch(level) {
		case PHP_ASYNCLOG_LEVEL_ERROR:
			levelstr = "error";
			break;
		case PHP_ASYNCLOG_LEVEL_WARN:
			levelstr = "warn";
			break;
		case PHP_ASYNCLOG_LEVEL_INFO:
			levelstr = "info";
			break;
		case PHP_ASYNCLOG_LEVEL_DEBUG:
			levelstr = "debug";
			break;
		case PHP_ASYNCLOG_LEVEL_VERBOSE:
			levelstr = "verbose";
			break;
		default:
			RETURN_FALSE;
	}

	t = (itertime - ASYNCLOG_G(itertime)) * 1000;

	ASYNCLOG_G(itertime) = itertime;

	if(duration < 0) {
		duration = t;
	}

	if(data) {
		if(Z_TYPE_P(data) == IS_STRING) {
			log_push(name, category, levelstr, message, Z_STR_P(data), timestamp, duration);
		} else if(php_json_encode(&buf, data, PHP_JSON_PRETTY_PRINT | PHP_JSON_UNESCAPED_UNICODE | PHP_JSON_UNESCAPED_SLASHES) == FAILURE) {
			smart_str_free(&buf);

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
		} else if (buf.s) {
			smart_str_0(&buf);

			// SYSLOG("[%s][%s][%s] %.3fms %s %s", name, category, levelstr, t, message, ZSTR_VAL(buf.s));
			log_push(name, category, levelstr, message, buf.s, timestamp, duration);

			smart_str_free(&buf);
		} else {
			// SYSLOG("[%s][%s][%s] %.3fms %s", name, category, levelstr, t, message);
			log_push(name, category, levelstr, message, NULL, timestamp, duration);
		}
	} else {
		// SYSLOG("[%s][%s][%s] %.3fms %s", name, category, levelstr, t, message);
		log_push(name, category, levelstr, message, NULL, timestamp, duration);
	}


#if ASYNCLOG_DEBUG
	if(!sapi_module.phpinfo_as_text) {
		sapi_header_line ctr = {0};

		ctr.line_len = spprintf(&ctr.line, 0, "ASYNCLOG: %ld", ++ASYNCLOG_G(logs));
		ctr.response_code = 0;

		sapi_header_op(SAPI_HEADER_REPLACE, &ctr);
	}
#endif

	RETURN_TRUE;
}

PHP_FUNCTION(asynclog_shutdown) {
	const int keys[] = {TRACK_VARS_POST, TRACK_VARS_GET, TRACK_VARS_COOKIE, TRACK_VARS_SERVER, TRACK_VARS_ENV, TRACK_VARS_FILES};
	const char *vars[] = {"_POST", "_GET", "_COOKIE", "_SERVER", "_ENV", "_FILES"};
	register int i;
	zval *var;
	zval globals;

	array_init_size(&globals, 5);
	for(i=0; i<sizeof(keys)/sizeof(const int); i++) {
		var = &PG(http_globals)[keys[i]];
		// var = zend_hash_str_find(&EG(symbol_table), vars[i], strlen(vars[i]));
		if(Z_TYPE_P(var) == IS_ARRAY && zend_hash_num_elements(Z_ARRVAL_P(var))) {
			 Z_ADDREF_P(var);
			add_assoc_zval(&globals, vars[i], var);
		}
	}

	if(php_json_encode(&ASYNCLOG_G(globals), &globals, PHP_JSON_PRETTY_PRINT | PHP_JSON_UNESCAPED_UNICODE | PHP_JSON_UNESCAPED_SLASHES) == FAILURE) {
		switch(JSON_G(error_code)) {
			case PHP_JSON_ERROR_NONE:
				SYSLOG("GLOBALS: No error");
				break;
			case PHP_JSON_ERROR_DEPTH:
				SYSLOG("GLOBALS: Maximum stack depth exceeded");
				break;
			case PHP_JSON_ERROR_STATE_MISMATCH:
				SYSLOG("GLOBALS: State mismatch (invalid or malformed JSON)");
				break;
			case PHP_JSON_ERROR_CTRL_CHAR:
				SYSLOG("GLOBALS: Control character error, possibly incorrectly encoded");
				break;
			case PHP_JSON_ERROR_SYNTAX:
				SYSLOG("GLOBALS: Syntax error");
				break;
			case PHP_JSON_ERROR_UTF8:
				SYSLOG("GLOBALS: Malformed UTF-8 characters, possibly incorrectly encoded");
				break;
			case PHP_JSON_ERROR_RECURSION:
				SYSLOG("GLOBALS: Recursion detected");
				break;
			case PHP_JSON_ERROR_INF_OR_NAN:
				SYSLOG("GLOBALS: Inf and NaN cannot be JSON encoded");
				break;
			case PHP_JSON_ERROR_UNSUPPORTED_TYPE:
				SYSLOG("GLOBALS: Type is not supported");
				break;
			case PHP_JSON_ERROR_INVALID_PROPERTY_NAME:
				SYSLOG("GLOBALS: The decoded property name is invalid");
				break;
			case PHP_JSON_ERROR_UTF16:
				SYSLOG("GLOBALS: Single unpaired UTF-16 surrogate in unicode escape");
				break;
			default:
				SYSLOG("GLOBALS: Unknown error");
				break;
		}
	} else if(ASYNCLOG_G(globals).s) {
		smart_str_0(&ASYNCLOG_G(globals));
	} else {
		SYSLOG("GLOBALS: Is empty");
	}

	zval_ptr_dtor(&globals);
}

PHP_GINIT_FUNCTION(asynclog) {
	OPENLOG();

	SYSLOG("GINIT");

	asynclog_globals->reqtime    = 0;
	asynclog_globals->restime    = 0;

	asynclog_globals->type       = 0;
	asynclog_globals->level      = 0;
	asynclog_globals->filepath   = NULL;
	asynclog_globals->redis_host = NULL;
	asynclog_globals->redis_port = 1;
	asynclog_globals->redis_auth = NULL;
	asynclog_globals->elastic    = NULL;

	asynclog_globals->output_len = 0;
	asynclog_globals->max_output = 0;
	asynclog_globals->is_shutdown= 0;
	asynclog_globals->is_signal  = 0;

	memset(&asynclog_globals->globals, sizeof(smart_str), 0);
	memset(&asynclog_globals->output, sizeof(smart_str), 0);
}

PHP_GSHUTDOWN_FUNCTION(asynclog) {
	SYSLOG("GSHUTDOWN");

	CLOSELOG();
}

PHP_MINIT_FUNCTION(asynclog) {
	SYSLOG("MINIT");

	REGISTER_STRING_CONSTANT("ASYNCLOG_VERSION",     PHP_ASYNCLOG_VERSION,       CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("ASYNCLOG_MODE_FILE",     PHP_ASYNCLOG_MODE_FILE,     CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("ASYNCLOG_MODE_REDIS",    PHP_ASYNCLOG_MODE_REDIS,    CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("ASYNCLOG_MODE_ELASTIC",  PHP_ASYNCLOG_MODE_ELASTIC,  CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("ASYNCLOG_LEVEL_ERROR",   PHP_ASYNCLOG_LEVEL_ERROR,   CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("ASYNCLOG_LEVEL_WARN",    PHP_ASYNCLOG_LEVEL_WARN,    CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("ASYNCLOG_LEVEL_INFO",    PHP_ASYNCLOG_LEVEL_INFO,    CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("ASYNCLOG_LEVEL_DEBUG",   PHP_ASYNCLOG_LEVEL_DEBUG,   CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("ASYNCLOG_LEVEL_VERBOSE", PHP_ASYNCLOG_LEVEL_VERBOSE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("ASYNCLOG_LEVEL_ALL",     PHP_ASYNCLOG_LEVEL_ALL,     CONST_CS | CONST_PERSISTENT);

	REGISTER_INI_ENTRIES();

	sapi_register_default_post_reader(json_post_reader);
	sapi_register_post_entries(php_post_entries);

	old_treat_data = sapi_module.treat_data;
	sapi_register_treat_data(restful_treat_data);

	{
		const zend_string *post = zend_string_init("_POST", sizeof("_POST")-1, 0);
		zend_hash_del(CG(auto_globals), post);
		zend_register_auto_global(post, 0, php_auto_globals_create_post);
		zend_string_free(post);
	}

	INILOG(MINIT);

//	if(sapi_module.phpinfo_as_text) {
//		log_init();
//	}

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(asynclog) {
	SYSLOG("MSHUTDOWN");
	INILOG(MSHUTDOWN);

	UNREGISTER_INI_ENTRIES();

	log_destroy();

	return SUCCESS;
}

PHP_RINIT_FUNCTION(asynclog) {
#if defined(COMPILE_DL_ASYNCLOG) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	ASYNCLOG_G(reqtime) = ASYNCLOG_G(itertime) = microtime();

	SYSLOG("RINIT: %f\n", ASYNCLOG_G(reqtime));

	INILOG(RINIT);

	if(!sapi_module.phpinfo_as_text) {
		ASYNCLOG_G(output_len) = 0;
		php_output_start_internal(ZEND_STRL("asynclog"), php_asynclog_output_handler, 0, PHP_OUTPUT_HANDLER_STDFLAGS);
	}

	// log_begin_request();

#if ASYNCLOG_DEBUG
	if(!sapi_module.phpinfo_as_text) {
		sapi_header_line ctr = {0};

		ctr.line_len = spprintf(&ctr.line, 0, "PID: %d/%d/%d", getpid(), getppid(), gettid());
		ctr.response_code = 0;

		sapi_header_op(SAPI_HEADER_REPLACE, &ctr);
		
		ASYNCLOG_G(logs) = 0;
	}
#endif

	if(SG(request_info).headers_only) {
		sapi_header_line ctr = {0};

		ctr.line_len = spprintf(&ctr.line, 0, "Content-Length: 0");
		ctr.response_code = 0;

		sapi_header_op(SAPI_HEADER_ADD, &ctr);
	}

	if(!ASYNCLOG_G(is_signal) && !strcmp(sapi_module.name, "fpm-fcgi")) {
		zend_signal(SIGUSR1, sig_handler);
		zend_signal(SIGUSR2, sig_handler);
		zend_signal(SIGTERM, sig_handler);
		zend_signal(SIGQUIT, sig_handler);
		ASYNCLOG_G(is_signal) = 1;
	}

	zend_is_auto_global_str(ZEND_STRL("_SERVER"));

	{
		/* create shutdown function */
		php_shutdown_function_entry shutdown_function_entry;
		shutdown_function_entry.arg_count = 1;
		shutdown_function_entry.arguments = (zval *) safe_emalloc(sizeof(zval), 1, 0);

		ZVAL_STRING(&shutdown_function_entry.arguments[0], "asynclog_shutdown");

		/* add shutdown function, removing the old one if it exists */
		if (!register_user_shutdown_function("asynclog_shutdown", sizeof("asynclog_shutdown") - 1, &shutdown_function_entry)) {
			zval_ptr_dtor(&shutdown_function_entry.arguments[0]);
			efree(shutdown_function_entry.arguments);
			php_error_docref(NULL, E_WARNING, "Unable to register asynclog shutdown function");
		}
	}

	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(asynclog) {
	ASYNCLOG_G(restime) = microtime();

	SYSLOG("RSHUTDOWN BEGIN: %f - %.3fms\n", ASYNCLOG_G(restime), (ASYNCLOG_G(restime) - ASYNCLOG_G(reqtime)) * 1000);

	INILOG(RSHUTDOWN);

	smart_str_0(&ASYNCLOG_G(output));

	remove_user_shutdown_function("asynclog_shutdown", sizeof("asynclog_shutdown") - 1);

	smart_str hbuf = {0}, rbuf = {0};
	zend_long i;

	HashTable *_SERVER = Z_ARRVAL(PG(http_globals)[TRACK_VARS_SERVER]);
	const char *ctlname;
	int status = 0;
	zend_string *post_data_str = NULL;

	if(!SG(request_info).request_uri) {
		zval *prog, *argv, *argc, *arg0;

		status = EG(exit_status);

		prog = zend_hash_str_find(_SERVER, "_", sizeof("_")-1);
		argv = zend_hash_str_find(_SERVER, "argv", sizeof("argv")-1);
		argc = zend_hash_str_find(_SERVER, "argc", sizeof("argc")-1);

		if(Z_LVAL_P(argc) == 0) {
			smart_str_append_ex(&rbuf, Z_STR_P(prog), 0);
		} else {
			arg0 = zend_hash_index_find(Z_ARRVAL_P(argv), 0);

			if(strcmp(Z_STRVAL_P(prog), Z_STRVAL_P(arg0))) {
				smart_str_append_ex(&rbuf, Z_STR_P(prog), 0);
				smart_str_appendc_ex(&rbuf, ' ', 0);
			}

			smart_str_appendc_ex(&rbuf, '\'', 0);
			smart_str_append_ex(&rbuf, Z_STR_P(arg0), 0);
			smart_str_appendc_ex(&rbuf, '\'', 0);

			for(i=1; i<Z_LVAL_P(argc); i++) {
				zval *tmp = zend_hash_index_find(Z_ARRVAL_P(argv), i);
				smart_str_appendc_ex(&rbuf, ' ', 0);
				smart_str_appendc_ex(&rbuf, '\'', 0);
				smart_str_append_ex(&rbuf, Z_STR_P(tmp), 0);
				smart_str_appendc_ex(&rbuf, '\'', 0);
			}
		}

		ctlname = "COMMAND";
	} else {
		zval *method, *scheme, *host, *url, *user_agent;

		status = SG(sapi_headers).http_response_code;

		zend_llist_apply_with_argument(&SG(sapi_headers).headers, php_head_apply_header_list_to_hash, &hbuf);

		method = zend_hash_str_find(_SERVER, "REQUEST_METHOD", sizeof("REQUEST_METHOD")-1);
		scheme = zend_hash_str_find(_SERVER, "REQUEST_SCHEME", sizeof("REQUEST_SCHEME")-1);
		host = zend_hash_str_find(_SERVER, "HTTP_HOST", sizeof("HTTP_HOST")-1);
		url = zend_hash_str_find(_SERVER, "REQUEST_URI", sizeof("REQUEST_URI")-1);
		user_agent = zend_hash_str_find(_SERVER, "HTTP_USER_AGENT", sizeof("HTTP_USER_AGENT")-1);

		smart_str_append_ex(&rbuf, Z_STR_P(method), 0);
		smart_str_appendc_ex(&rbuf, ' ', 0);
		smart_str_append_ex(&rbuf, Z_STR_P(scheme), 0);
		smart_str_appendl_ex(&rbuf, "://", 3, 0);
		smart_str_append_ex(&rbuf, Z_STR_P(host), 0);
		smart_str_append_ex(&rbuf, Z_STR_P(url), 0);
		if(user_agent) {
			smart_str_appendl_ex(&rbuf, " \"", 2, 0);
			smart_str_append_ex(&rbuf, Z_STR_P(user_agent), 0);
			smart_str_appendc_ex(&rbuf, '"', 0);
		} else {
			smart_str_appendl_ex(&rbuf, " \"-\"", 4, 0);
		}

		ctlname = "REQUEST";
	}

	smart_str_0(&rbuf);
	smart_str_0(&hbuf);

	if(SG(request_info).request_body) {
		php_stream_rewind(SG(request_info).request_body);
		post_data_str = php_stream_copy_to_mem(SG(request_info).request_body, PHP_STREAM_COPY_ALL, 0);
	}

#if 0 // ASYNCLOG_DEBUG
	if(rbuf.s) {
		SYSLOG("%s: %s", ctlname, ZSTR_VAL(rbuf.s));
	}
	SYSLOG("STATUS: %d", status);
	if(hbuf.s) {
		SYSLOG("HEADERS: %s", ZSTR_VAL(hbuf.s) + strlen(PHP_EOL));
	}
	if(ASYNCLOG_G(globals).s) {
		SYSLOG("GLOBALS: %s", ZSTR_VAL(ASYNCLOG_G(globals).s));
	}
	if(SG(request_info).content_length) {
		SYSLOG("CONTENT_TYPE: %s", SG(request_info).content_type);
		SYSLOG("CONTENT_LENGTH: %ld", SG(request_info).content_length);
	}
	if(post_data_str) {
		SYSLOG("POST_DATA: %s", ZSTR_VAL(post_data_str));
	}
	if(ASYNCLOG_G(output_len)) {
		SYSLOG("OUTPUT_LEN: %ld", ASYNCLOG_G(output_len));
		SYSLOG("OUTPUT: %s", ZSTR_VAL(ASYNCLOG_G(output).s));
	}
#else
	if(rbuf.s) {
		SYSLOG("%s: %s", ctlname, ZSTR_VAL(rbuf.s));
	}
#endif

	log_end_request(ctlname, rbuf.s, ASYNCLOG_G(globals).s, SG(request_info).content_type, SG(request_info).content_length, status, hbuf.s, ASYNCLOG_G(output).s, post_data_str);

	if(post_data_str) {
		zend_string_free(post_data_str);
	}

	smart_str_free(&ASYNCLOG_G(globals));
	smart_str_free(&ASYNCLOG_G(output));
	smart_str_free(&hbuf);
	smart_str_free(&rbuf);

#if ASYNCLOG_DEBUG
	{
		double t = microtime();
		SYSLOG("RSHUTDOWN END: %f - %.3fms\n", t, (t - ASYNCLOG_G(restime)) * 1000);
	}
#endif

	if(ASYNCLOG_G(is_shutdown)) {
		SYSLOG("FCGI SHUTDOWN");

		log_destroy();
	}

	return SUCCESS;
}

PHP_MINFO_FUNCTION(asynclog) {
	SYSLOG("MINFO");
	INILOG(MINFO);

	php_info_print_table_start();
	php_info_print_table_header(2, "asynclog support", "enabled");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}

static int php_asynclog_post_deactivate(void) {
	INILOG(POST_DEACTIVATE);
	return SUCCESS;
}

const zend_function_entry asynclog_functions[] = {
	PHP_FE(asynclog, arginfo_asynclog)
	PHP_FE(asynclog_shutdown, arginfo_asynclog_shutdown)
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
	php_asynclog_post_deactivate,
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
