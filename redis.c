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

#define REDIS_FLAG_OK    '+'
#define REDIS_FLAG_ERR   '-'
#define REDIS_FLAG_INT   ':'
#define REDIS_FLAG_BULK  '$'
#define REDIS_FLAG_MULTI '*'

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

int redis_debug(redis_t *redis) {
	assert(redis);

	redis->flag |= REDIS_FLAG_DEBUG;

	return REDIS_TRUE;
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

#define REDIS_DEBUG if((redis->flag & REDIS_FLAG_DEBUG) != 0)

#define SEND(fmt, args...) \
	do { \
		REDIS_DEBUG printf("> " fmt "\n", ##args); \
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

	REDIS_DEBUG printf("====================================================================================\n");

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

#define ERRMSG_X(sock, msg) \
	if((sock) <= 0) { \
		perror(msg); \
		return REDIS_FALSE; \
	}

static int dgets(redis_t *redis) {
	register int n = 0, end = 0;
	char *p = redis->buf;

	while(end != 2 && ++n < sizeof(redis->buf)) {
		ERRMSG_X(recv(redis->fd, p, 1, MSG_WAITALL), "RECV");
		if(*p == '\r') {
			end = 1;
		} else if(*p == '\n') {
			end = 2;
		}
		*(++p) = '\0';
	}

	if(end != 2) {
		fprintf(stderr, "UNCOMPLETE\n");
	}

	return REDIS_TRUE;
}

int redis_recv(redis_t *redis, char flag) {
	assert(redis->fd > 0);

	REDIS_DEBUG printf("------------------------------------------------------------------------------------\n");

	redis_clean(redis);

	redis->c = '\0';

	int ret, len, n = 1, i = 0;
	char c = 0, c2, *p;
	for(; n > 0; n--) {
		if(!dgets(redis)) return REDIS_FALSE;
		REDIS_DEBUG printf("< %s", redis->buf);

		c2 = redis->buf[0];

		if(!c) {
			c = c2;
			redis->c = c2;
		}

		switch(c2) {
			case '*':
				n = atoi(redis->buf + 1);
				len = sizeof(str_t) * n;
				redis->argc = n;
				redis->argv = (str_t*) malloc(len);
				memset(redis->argv, 0, len);
				n++;
				break;
			case '$':
				if(redis->argc == 0) {
					redis->argc = 1;
					redis->argv = (str_t*) malloc(sizeof(str_t));
				}
				len = atoi(redis->buf + 1);
				redis->argv[i].len = len;
				redis->argv[i].str = (char*) malloc(sizeof(char)*len+2);
				if(len > 0) {
					REDIS_DEBUG printf("< ");
				} else {
					i++;
					break;
				}
				ret = recv(redis->fd, redis->argv[i].str, len+2, MSG_WAITALL);
				ERRMSG_X(ret, "RECV");
				REDIS_DEBUG {
					fwrite(redis->argv[i].str, 1, ret, stdout);
				}
				i++;
				break;
			case ':':
			case '+':
				goto end;
			case '-':
				goto end;
				break;
			default:
				fprintf(stderr, "Unkown REDIS response data type '%c'\n", c);
				return REDIS_FALSE;
		}
	}

end:
	return (flag == c || flag == '\0') ? REDIS_TRUE : REDIS_FALSE;
}

int redis_senda(redis_t *redis, int argc, char *argv[]) {
	int i, n;
	char *buf = redis->buf;

	REDIS_DEBUG printf("====================================================================================\n");

	SEND("*%d", argc);

	for(i=0; i<argc; i++) {
		n = strlen(argv[i]);
		SEND("$%d", n);
		SEND("%s", argv[i]);
	}

	SEND("");

	ERRMSG(send(redis->fd, redis->buf, buf-redis->buf, 0), "SEND");
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
	if(redis->argc && redis->argv) {
		int i;
		for(i=0; i<redis->argc; i++) {
			if(redis->argv[i].str) free(redis->argv[i].str);
		}
		free(redis->argv);

		redis->argc = 0;
		redis->argv = NULL;
	}
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
