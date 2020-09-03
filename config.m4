PHP_ARG_ENABLE(asynclog, whether to enable asynclog support, [  --enable-asynclog           Enable asynclog support])

if test "$PHP_ASYNCLOG" != "no"; then
  PHP_NEW_EXTENSION(asynclog, asynclog.c api.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi
