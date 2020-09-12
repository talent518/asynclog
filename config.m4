dnl
dnl PHP_SELECT_PROGRAM(name, sources [, extra-cflags [, build-target]]])
dnl
AC_DEFUN([PHP_SELECT_PROGRAM],[
  PHP_BINARIES="$PHP_BINARIES $1"
  PHP_INSTALLED_SAPIS="$PHP_INSTALLED_SAPIS $1"

  PHP_BUILD_PROGRAM
  install_binaries="install-binaries"
  install_binary_targets="$install_binary_targets install-$1"
  PHP_SUBST(PHP_[]translit($1,a-z0-9-,A-Z0-9_)[]_OBJS)
  ifelse($2,,,[PHP_ADD_SOURCES_X([$ext_dir],[$2],[$3],PHP_[]translit($1,a-z0-9-,A-Z0-9_)[]_OBJS)])
])

PHP_ARG_ENABLE(debug, whether to enable syslog debug support, [  --enable-debug              Enable syslog debug support], [no])
PHP_ARG_ENABLE(asynclog, whether to enable asynclog support,  [  --enable-asynclog           Enable asynclog support])
PHP_ARG_ENABLE(redis, whether to enable redis support,        [  --enable-redis              Enable redis support], [no])

if test "$PHP_ASYNCLOG" != "no"; then
	CFLAGS="$CFLAGS -DASYNCLOG_DEBUG"
fi

dnl CFLAGS="$CFLAGS -Wno-discarded-qualifiers"

PHP_EVAL_LIBLINE([-lm], LDFLAGS)
PHP_ADD_SOURCES([$ext_dir], api.c redis.c, , global)
PHP_SUBST(PHP_GLOBAL_OBJS)

if test "$PHP_ASYNCLOG" != "no"; then
  PHP_NEW_EXTENSION(asynclog, asynclog.c log.c log_file.c log_redis.c log_elastic.c, $ext_shared, , -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
  shared_objects_asynclog="$shared_objects_asynclog \$(PHP_GLOBAL_OBJS)"
  ASYNCLOG_SHARED_LIBADD="-pthread"
  PHP_SUBST(ASYNCLOG_SHARED_LIBADD)
fi

if test "$PHP_REDIS" != "no"; then
  PHP_ADD_MAKEFILE_FRAGMENT($abs_srcdir/Makefile.frag)
  
  PHP_SELECT_PROGRAM(redis, asynclog-redis.c, , 'redis')
  
  case $host_alias in
    *aix*)
      if test "$php_sapi_module" = "shared"; then
        BUILD_REDIS="echo '\#! .' > php.sym && echo >>php.sym && nm -BCpg \`echo \$(PHP_GLOBAL_OBJS:.lo=.o) \$(PHP_REDIS_OBJS) | sed 's/\([A-Za-z0-9_]*\)\.lo/.libs\/\1.o/g'\` | \$(AWK) '{ if (((\$\$2 == \"T\") || (\$\$2 == \"D\") || (\$\$2 == \"B\")) && (substr(\$\$3,1,1) != \".\")) { print \$\$3 } }' | sort -u >> php.sym && \$(LIBTOOL) --mode=link \$(CC) -export-dynamic \$(CFLAGS_CLEAN) \$(EXTRA_CFLAGS) \$(EXTRA_LDFLAGS_PROGRAM) \$(LDFLAGS) -Wl,-brtl -Wl,-bE:php.sym \$(PHP_RPATHS) \$(PHP_GLOBAL_OBJS) \$(PHP_REDIS_OBJS) \$(EXTRA_LIBS) \$(ZEND_EXTRA_LIBS) -o redis"
      else
        BUILD_REDIS="echo '\#! .' > php.sym && echo >>php.sym && nm -BCpg \`echo \$(PHP_GLOBAL_OBJS:.lo=.o) \$(PHP_REDIS_OBJS) | sed 's/\([A-Za-z0-9_]*\)\.lo/\1.o/g'\` | \$(AWK) '{ if (((\$\$2 == \"T\") || (\$\$2 == \"D\") || (\$\$2 == \"B\")) && (substr(\$\$3,1,1) != \".\")) { print \$\$3 } }' | sort -u >> php.sym && \$(LIBTOOL) --mode=link \$(CC) -export-dynamic \$(CFLAGS_CLEAN) \$(EXTRA_CFLAGS) \$(EXTRA_LDFLAGS_PROGRAM) \$(LDFLAGS) -Wl,-brtl -Wl,-bE:php.sym \$(PHP_RPATHS) \$(PHP_GLOBAL_OBJS) \$(PHP_REDIS_OBJS) \$(EXTRA_LIBS) \$(ZEND_EXTRA_LIBS) -o redis"
      fi
      ;;
    *darwin*)
      BUILD_REDIS="\$(CC) \$(CFLAGS_CLEAN) \$(EXTRA_CFLAGS) \$(EXTRA_LDFLAGS_PROGRAM) \$(LDFLAGS) \$(NATIVE_RPATHS) \$(PHP_GLOBAL_OBJS:.lo=.o) \$(PHP_REDIS_OBJS:.lo=.o) \$(PHP_FRAMEWORKS) \$(EXTRA_LIBS) \$(ZEND_EXTRA_LIBS) -o redis"
    ;;
    *)
      BUILD_REDIS="\$(LIBTOOL) --mode=link \$(CC) -export-dynamic \$(CFLAGS_CLEAN) \$(EXTRA_CFLAGS) \$(EXTRA_LDFLAGS_PROGRAM) \$(LDFLAGS) \$(PHP_RPATHS) \$(PHP_GLOBAL_OBJS) \$(PHP_REDIS_OBJS) \$(EXTRA_LIBS) \$(ZEND_EXTRA_LIBS) -o redis"
    ;;
  esac

  dnl Expose to Makefile.
  PHP_SUBST(BUILD_REDIS)
fi

all_targets="$all_targets build-binaries"
install_targets="$install_targets install-binaries"

PHP_SUBST(all_targets)
PHP_SUBST(install_targets)
PHP_SUBST(PHP_BINARIES)
PHP_SUBST(PHP_INSTALLED_SAPIS)

