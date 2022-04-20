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
#include <unistd.h>

#include "redis.h"

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

	if(connect(redis->fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in))) {
		perror("CONNECT");
		return REDIS_FALSE;
	}

	redis->fp = fdopen(redis->fd, "rw+");
	if(redis->fp == NULL) {
		perror("FDOPEN");
		return REDIS_FALSE;
	}

	return REDIS_TRUE;
}

char* redis_error(redis_t *redis) {
	if(redis->data.c == '-') return redis->data.str;
	else "Unknown";
}

#define REDIS_DEBUG if((redis->flag & REDIS_FLAG_DEBUG) != 0)

#define CASE(chr, c, fmt, type) \
	case chr : { \
		type x##c = (type) va_arg(ap, type); \
		n = snprintf(dbuf, sizeof(dbuf), fmt, x##c); \
		REDIS_DEBUG printf("> $%d\n> %s\n", n, dbuf); \
		fprintf(redis->fp, "$%d\r\n%s\r\n", n, dbuf); \
		break; \
	}

int redis_send(redis_t *redis, const char *format, ...) {
	assert(redis->fd > 0);
	assert(redis->fp);

	REDIS_DEBUG printf("====================================================================================\n");

	va_list ap;
	int n;
	char dbuf[32];
	const char *p = format, *ptr;

	n = strlen(format);

	REDIS_DEBUG printf("> *%d\n", n);
	fprintf(redis->fp, "*%d\r\n", n);

	va_start(ap, format);
	while (*p) {
		switch(*p) {
			case 's': {
				ptr = va_arg(ap, const char*);
				if(ptr == NULL) ptr = "";
				n = strlen(ptr);
				REDIS_DEBUG printf("> $%d\n> %s\n", n, ptr);
				fprintf(redis->fp, "$%d\r\n%s\r\n", n, ptr);
				break;
			}
			case 'S': {
				ptr = va_arg(ap, const char*);
				if(ptr == NULL) ptr = "";
				n = va_arg(ap, int);
				REDIS_DEBUG {
					printf("> $%d\n> ", n);
					fwrite(ptr, 1, n, stdout);
					printf("\n");
				}
				fprintf(redis->fp, "$%d\r\n", n);
				fwrite(ptr, 1, n, redis->fp);
				fprintf(redis->fp, "\r\n");
				break;
			}
			CASE('d', d, "%d", int)
			CASE('D', D, "%ld", long int)
			CASE('f', f, "%lg", double)
			CASE('F', F, "%lg", double)
			CASE('u', u, "%u", unsigned int)
			CASE('U', U, "%lu", unsigned long int)
			default:
				break;
		}
		p++;
	}
	va_end(ap);

	if(fflush(redis->fp)) {
		perror("SEND");
		return REDIS_FALSE;
	} else {
		return REDIS_TRUE;
	}
}

int redis_senda(redis_t *redis, int argc, const char *argv[]) {
	int i, n;

	REDIS_DEBUG printf("====================================================================================\n");

	REDIS_DEBUG printf("*%d\n", argc);
	fprintf(redis->fp, "*%d\r\n", argc);

	for(i=0; i<argc; i++) {
		n = strlen(argv[i]);
		REDIS_DEBUG printf("> $%d\n> %s\n", n, argv[i]);
		fprintf(redis->fp, "$%d\r\n%s\r\n", n, argv[i]);
	}

	if(fflush(redis->fp)) {
		perror("SEND");
		return REDIS_FALSE;
	} else {
		return REDIS_TRUE;
	}
}

int redis_dgets(redis_t *redis) {
	if(fgets(redis->buf, sizeof(redis->buf), redis->fp) == NULL) {
		perror("FGETS");
		return REDIS_FALSE;
	}

	register char *p = redis->buf;

	while(*p && *p != '\r' && *p != '\n') p++;

	*p = '\0';

	REDIS_DEBUG printf("< %s\n", redis->buf);

	return REDIS_TRUE;
}

static int _redis_recv(redis_t *redis, redis_data_t *data) {
	int i;
	switch(data->c) {
		case REDIS_FLAG_MULTI:
			data->sz = atoi(redis->buf + 1);
			if(data->sz <= 0) return REDIS_TRUE;
			data->data = (redis_data_t*) malloc(sizeof(redis_data_t)*data->sz);
			memset(data->data, 0, sizeof(redis_data_t)*data->sz);
			for(i=0; i<data->sz; i++) {
				if(!redis_dgets(redis)) return REDIS_FALSE;
				data->data[i].c = redis->buf[0];
				if(!_redis_recv(redis, &data->data[i])) {
					break;
				}
			}
			break;
		case REDIS_FLAG_BULK:
			data->sz = atoi(redis->buf + 1);
			if(data->sz >= 0) {
				data->str = (char*) malloc(data->sz+2);
				i = fread(data->str, 1, data->sz+2, redis->fp);
				if(i != data->sz+2) {
					if(i > 0) {
						data->str[i] = '\0';
						REDIS_DEBUG {
							printf("< ");
							fwrite(data->str, 1, i, stdout);
							printf("\n");
						}
					}
					perror("FREAD");
					return REDIS_FALSE;
				}
				data->str[data->sz] = '\0';
				REDIS_DEBUG {
					printf("< ");
					fwrite(data->str, 1, data->sz, stdout);
					printf("\n");
				}
			}
			break;
		case REDIS_FLAG_INT:
			data->l = strtol(redis->buf + 1, NULL, 10);
			break;
		case REDIS_FLAG_ERR:
			data->str = strdup(redis->buf + 5);
			data->sz = strlen(data->str);
			break;
		case REDIS_FLAG_OK:
			data->str = strdup(redis->buf + 1);
			data->sz = strlen(data->str);
			break;
		default:
			printf("===%x===\n", data->c);
			return REDIS_FALSE;
	}
	return REDIS_TRUE;
}

int redis_recv(redis_t *redis, char flag) {
	assert(redis->fd > 0);
	assert(redis->fp);

	REDIS_DEBUG printf("------------------------------------------------------------------------------------\n");

	redis_clean(redis);
	
	if(!redis_dgets(redis)) return REDIS_FALSE;

	redis->data.c = redis->buf[0];
	if(!_redis_recv(redis, &redis->data)) return REDIS_FALSE;

	return (flag == redis->data.c || flag == REDIS_FLAG_ANY || redis->data.c == REDIS_FLAG_OK) ? REDIS_TRUE : REDIS_FALSE;
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

int redis_dbsize(redis_t *redis, long int *size) {
	if(!redis_send(redis, "s", "DBSIZE")) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_INT)) return REDIS_FALSE;
	if(size) *size = (redis->data.c == REDIS_FLAG_INT ? redis->data.l : 0);
	return REDIS_TRUE;
}

int redis_keys(redis_t *redis, const char *pattern, char ***keys) {
	if(!redis_send(redis, "ss", "KEYS", pattern)) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_MULTI)) return REDIS_FALSE;
	if(keys && redis->data.c == REDIS_FLAG_MULTI && redis->data.sz > 0) {
		int offset = sizeof(char*) * (redis->data.sz + 1);
		int size = offset + redis->data.sz;
		int i;
		for(i=0; i<redis->data.sz; i++) {
			size += redis->data.data[i].sz;
		}

		char **ptr = (char**) malloc(size);
		char *p = (char*) ptr + offset;
		for(i=0; i<redis->data.sz; i++) {
			ptr[i] = p;
			memcpy(p, redis->data.data[i].str, redis->data.data[i].sz);
			p += redis->data.data[i].sz + 1;
			*(p-1) = '\0';
		}
		ptr[i] = NULL;
		*keys = ptr;
	}
	return REDIS_TRUE;
}

int redis_quit(redis_t *redis) {
	if(!redis_send(redis, "s", "QUIT")) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_OK)) return REDIS_FALSE;
	return REDIS_TRUE;
}

int redis_type(redis_t *redis, const char *key, char **rtype) {
	if(!redis_send(redis, "ss", "TYPE", key)) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_BULK)) return REDIS_FALSE;
	if(rtype && redis->data.c == REDIS_FLAG_BULK) {
		char *p;
		int n = strlen(redis->buf + 1);

		if(*rtype == NULL) {
			*rtype = (char*) malloc(n + 1);
		}
		p = *rtype;
		memcpy(p, redis->buf + 1, n);
		p[n] = '\0';
	}
	return REDIS_TRUE;
}

int redis_del(redis_t *redis, const char *key, int *exists) {
	if(!redis_send(redis, "ss", "DEL", key)) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_INT)) return REDIS_FALSE;
	if(exists) *exists = (redis->data.c == REDIS_FLAG_INT ? redis->data.l : 0);
	return REDIS_TRUE;
}

int redis_del_keys(redis_t *redis, const char *pattern, int *exists) {
	int ret = REDIS_FALSE, k = 0;
	char **keys = NULL;

	if(exists) *exists = 0;

	if(!redis_keys(redis, pattern, &keys)) goto end;
	
	if(keys) {
		for(k=0; keys[k]; k++);

		if(k > 0) {
			keys[k] = keys[0];
			keys[0] = "del";

			if(!redis_senda(redis, k+1, (const char **) keys)) goto end;
			if(!redis_recv(redis, REDIS_FLAG_INT)) goto end;

			if(exists) *exists = (redis->data.c == REDIS_FLAG_INT ? redis->data.l : 0);
		}
	}

	ret = REDIS_TRUE;

end:
	if(keys) free(keys);

	return ret;
}

int redis_set(redis_t *redis, const char *key, const char *value) {
	if(!redis_send(redis, "sss", "SET", key, value)) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_OK)) return REDIS_FALSE;
	return REDIS_TRUE;
}

int redis_get_ex(redis_t *redis, const char *key, char **value, int *size) {
	if(!redis_send(redis, "ss", "GET", key)) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_BULK)) return REDIS_FALSE;
	if(size) *size = (redis->data.c == REDIS_FLAG_BULK ? redis->data.sz : 0);
	if(value) {
		if(*value) free(*value);
		if(redis->data.c == REDIS_FLAG_BULK && redis->data.sz >= 0) {
			*value = redis->data.str;
			redis->data.sz = 0;
			redis->data.str = NULL;
		} else {
			*value = NULL;
		}
	}
	return REDIS_TRUE;
}

int redis_get(redis_t *redis, const char *key, char **value) {
	return redis_get_ex(redis, key, value, NULL);
}

int redis_incrby(redis_t *redis, const char *key, long int inc, long int *value) {
	if(!redis_send(redis, "ssD", "incrby", key, inc)) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_INT)) return REDIS_FALSE;
	if(value) *value = (redis->data.c == REDIS_FLAG_INT ? redis->data.l : 0);
	return REDIS_TRUE;
}

int redis_multi(redis_t *redis) {
	if(!redis_send(redis, "s", "MULTI")) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_OK)) return REDIS_FALSE;
	return REDIS_TRUE;
}

int redis_exec(redis_t *redis) {
	if(!redis_send(redis, "s", "EXEC")) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_MULTI)) return REDIS_FALSE;
	return REDIS_TRUE;
}

int redis_scan(redis_t *redis, int cursor, const char *match, int count) {
	if(!redis_send(redis, "sdsssd", "SCAN", cursor, "MATCH", match, "COUNT", count)) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_MULTI)) return REDIS_FALSE;
	return REDIS_TRUE;
}

void _redis_clean(redis_data_t *data) {
	int i;
	switch(data->c) {
		case '*':
			for(i=0; i<data->sz; i++) _redis_clean(&data->data[i]);
			if(data->sz > 0) {
				free(data->data);
				data->sz = 0;
				data->data = NULL;
			}
			break;
		case '\0':
		case ':':
			break;
		default:
			if(data->str) {
				free(data->str);
				data->sz = 0;
				data->str = NULL;
			}
			break;
	}
	memset(data, 0, sizeof(redis_data_t));
}

int redis_index_int(redis_t *redis, const char *key, int index, long int *value) {
	if(!redis_send(redis, "ssd", "lindex", key, index)) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_BULK)) return REDIS_FALSE;

	if(redis->data.c == REDIS_FLAG_BULK && redis->data.sz > 0) *value = strtol(redis->data.str, NULL, 10);
	else *value = 0;

	return REDIS_TRUE;
}

int redis_last_int(redis_t *redis, const char *key, long int *value) {
	return redis_index_int(redis, key, -1, value);
}

int redis_rpush_int(redis_t *redis, const char *key, long int value) {
	if(!redis_send(redis, "ssD", "rpush", key, value)) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_INT)) return REDIS_FALSE;

	return REDIS_TRUE;
}

int redis_lpop_int(redis_t *redis, const char *key, long int *value) {
	if(!redis_send(redis, "ss", "lpop", key)) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_BULK)) return REDIS_FALSE;

	if(redis->data.c == REDIS_FLAG_BULK && redis->data.sz > 0) *value = strtol(redis->data.str, NULL, 10);
	else *value = 0;

	return REDIS_TRUE;
}

int redis_blpop_int(redis_t *redis, const char *key, int timeout, long int *value) {
	if(!redis_send(redis, "ssd", "blpop", key, timeout)) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_MULTI)) return REDIS_FALSE;

	if(redis->data.c == REDIS_FLAG_MULTI && redis->data.sz == 2) {
		*value = strtol(redis->data.data[1].str, NULL, 10);
	} else *value = 0;

	return REDIS_TRUE;
}

int redis_lrem_int(redis_t *redis, const char *key, int count, long int value) {
	if(!redis_send(redis, "ssdD", "lrem", key, count, value)) return REDIS_FALSE;
	if(!redis_recv(redis, REDIS_FLAG_INT)) return REDIS_FALSE;

	return REDIS_TRUE;
}

int redis_lrem_keys(redis_t *redis, const char *pattern, long int value, int *exists) {
	int ret = REDIS_FALSE, k;
	char **keys = NULL;

	if(!redis_keys(redis, pattern, &keys)) goto end;
	
	if(keys) {
		for(k=0; keys[k]; k++) {
			if(!redis_lrem_int(redis, keys[k], 0, value)) goto end;
			if(exists && redis->data.c == REDIS_FLAG_INT) *exists += redis->data.l;
		}
	}

	ret = REDIS_TRUE;

end:
	if(keys) free(keys);

	return ret;
}

int redis_close(redis_t *redis) {
	redis_clean(redis);

	if(redis->fp) {
		fclose(redis->fp);

		redis->fp = NULL;
	}

	if(redis->fd > 0) {
#if 0
		if(shutdown(redis->fd, SHUT_RDWR)) {
			perror("SHUTDOWN");
			return REDIS_FALSE;
		}

		if(close(redis->fd)) {
			perror("CLOSE");
			return REDIS_FALSE;
		}
#endif

		redis->fd = 0;
	}

	return REDIS_TRUE;
}

void redis_destory(redis_t *redis) {
	assert(redis != NULL);

	redis_close(redis);

	if((redis->flag & REDIS_FLAG_FREE) == REDIS_FLAG_FREE) {
		free(redis);
	}
}
