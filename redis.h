#ifndef _REDIS_H
#define _REDIS_H

#define REDIS_TRUE 1
#define REDIS_FALSE 0
#define REDIS_VERSION "0.1"

#define REDIS_FLAG_FREE  1<<0
#define REDIS_FLAG_DEBUG 1<<1
#define REDIS_FLAG_ANY   0

typedef struct _str_t {
	int len;
	char *str;
} str_t;

typedef struct _redis_t {
	int fd;
	int flag;
	char ip[32];

	char buf[512];
	char c;

	int  argc;
	str_t *argv;
} redis_t;

redis_t *redis_init(redis_t *redis, int flag);

    int  redis_debug(redis_t *redis);

    int  redis_connect(redis_t *redis, const char *host, int port);
    int  redis_send(redis_t *redis, const char *format, ...);
    int  redis_recv(redis_t *redis, char flag);
    int  redis_senda(redis_t *redis, int argc, const char *argv[]);

    int  redis_auth(redis_t *redis, const char *password);
    int  redis_ping(redis_t *redis);
    int  redis_echo(redis_t *redis, const char *str);
    int  redis_select(redis_t *redis, int database);
    int  redis_dbsize(redis_t *redis, int *size);
    int  redis_keys(redis_t *redis, const char *pattern, char ***keys);
    int  redis_quit(redis_t *redis);

    int  redis_type(redis_t *redis, const char *type, char **rtype);

   void  redis_clean(redis_t *redis);
    int  redis_close(redis_t *redis);
   void  redis_destory(redis_t *redis);

#endif
