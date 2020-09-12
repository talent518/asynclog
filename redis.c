#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <assert.h>
#include <stdarg.h>
#include <netdb.h>

#include "redis.h"

#define ERRMSG(sock, msg) \
	if(sock < 0) { \
		perror(msg); \
		return REDIS_FALSE; \
	} else { \
		return REDIS_TRUE; \
	}

#define CRLF "\r\n"

redis_t *redis_init(redis_t *redis, int flag) {
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if(fd < 0) return NULL;

	if(redis == NULL) {
		redis = (redis_t*) malloc(sizeof(redis_t));
		flag |= REDIS_FLAG_FREE;
	}

	memset(redis, 0, sizeof(redis_t));

	redis->flag = flag;
	redis->fd = fd;

	return redis;
}

int redis_connect(redis_t *redis, const char *host, int port) {
	assert(redis->fd > 0);

	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(host);
	if(addr.sin_addr.s_addr == INADDR_NONE) {
		struct hostent *he = gethostbyname(host);
		memcpy(&addr.sin_addr, he->h_addr_list[0], sizeof(struct in_addr));
		strcpy(redis->ip, inet_ntoa(addr.sin_addr));
	} else {
		strcpy(redis->ip, host);
	}

	ERRMSG(connect(redis->fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)), "CONNECT");
}

#define SEND(fmt, args...) \
	do { \
		printf("> " fmt "\n", ##args); \
		buf += sprintf(buf, fmt CRLF, ##args); \
	} while(0)

#define CASE(chr, c, fmt, type) \
	case chr : { \
		type x##c = va_arg(ap, type); \
		n = snprintf(dbuf, sizeof(dbuf), fmt, x##c); \
		SEND("$%d", n); \
		SEND("%s", dbuf); \
		break; \
	}

int redis_send(redis_t *redis, const char *format, ...) {
	assert(redis->fd > 0);

	printf("====================================================================================\n");

	va_list ap;
	int ret, n;
	char *p;
	char *ptr, *buf = redis->buf;
	char dbuf[32];

	n = strlen(format);
	SEND("*%d", n);

	p = format;

	va_start(ap, format);
	while (*p) {
		switch(*p) {
			case 's': {
				ptr = va_arg(ap, char*);
				n = strlen(ptr);
				SEND("$%d", n);
				SEND("%s", ptr);
				break;
			}
			case 'S': {
				ptr = va_arg(ap, char*);
				n = va_arg(ap, int);
				SEND("$%d", n);
				SEND("%s", ptr);
				break;
			}
			CASE('d', d, "%d", int)
			CASE('D', D, "%ld", long int)
			CASE('f', f, "%f", float)
			CASE('F', F, "%lf", double)
			CASE('u', u, "%u", unsigned int)
			CASE('U', U, "%lu", unsigned long int)
			default:
				break;
		}
		p++;
	}
	va_end(ap);

	SEND("");

	ERRMSG(send(redis->fd, redis->buf, buf-redis->buf, 0), "SEND");
}

int redis_recv(redis_t *redis, int flag) {
	assert(redis->fd > 0);

	printf("------------------------------------------------------------------------------------\n");

	int ret = recv(redis->fd, redis->buf, sizeof(redis->buf), 0);

	if(ret > 0) {
		printf("< ");
		fwrite(redis->buf, 1, ret, stdout);
		return REDIS_TRUE;
	} else {
		perror("RECV");
		return REDIS_FALSE;
	}
}

int redis_auth(redis_t *redis, const char *password) {
	if(!redis_send(redis, "ss", "AUTH", password)) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_OK)) return REDIS_FALSE;
	return REDIS_TRUE;
}

int redis_ping(redis_t *redis) {
	if(!redis_send(redis, "s", "PING")) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_OK)) return REDIS_FALSE;
	return REDIS_TRUE;
}

int redis_echo(redis_t *redis, const char *str) {
	if(!redis_send(redis, "ss", "ECHO", str)) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_BULK)) return REDIS_FALSE;
	return REDIS_TRUE;
}

int redis_select(redis_t *redis, int database) {
	if(!redis_send(redis, "sd", "SELECT", database)) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_OK)) return REDIS_FALSE;
	return REDIS_TRUE;
}

int redis_dbsize(redis_t *redis, int *size) {
	if(!redis_send(redis, "s", "DBSIZE")) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_INT)) return REDIS_FALSE;
	return REDIS_TRUE;
}

int redis_keys(redis_t *redis, const char *pattern, char **keys) {
	if(!redis_send(redis, "ss", "KEYS", pattern)) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_MULTI)) return REDIS_FALSE;
	return REDIS_TRUE;
}

int redis_quit(redis_t *redis) {
	if(!redis_send(redis, "s", "QUIT")) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_OK)) return REDIS_FALSE;
	return REDIS_TRUE;
}

int redis_type(redis_t *redis, const char *type) {
	if(!redis_send(redis, "ss", "TYPE", type)) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_OK)) return REDIS_FALSE;
	return REDIS_TRUE;
}

void redis_clean(redis_t *redis) {
}

int redis_close(redis_t *redis) {
	assert(redis->fd > 0);

	ERRMSG(shutdown(redis->fd, SHUT_RDWR), "SHUTDOWN");
	ERRMSG(close(redis->fd), "CLOSE");
}

void redis_destory(redis_t *redis) {
	assert(redis != NULL);

	redis_clean(redis);

	if((redis->flag & REDIS_FLAG_FREE) == REDIS_FLAG_FREE) {
		free(redis);
	}
}
