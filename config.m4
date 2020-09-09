PHP_ARG_ENABLE(asynclog, whether to enable asynclog support, [  --enable-asynclog           Enable asynclog support])

if test "$PHP_ASYNCLOG" != "no"; then
  PHP_EVAL_LIBLINE([-pthread], LDFLAGS)
  PHP_NEW_EXTENSION(asynclog, asynclog.c api.c log.c log_file.c log_redis.c log_elastic.c, $ext_shared, , -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi
