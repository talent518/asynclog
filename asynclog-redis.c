#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#include "redis.h"

void _print_data(redis_data_t *data, int indent, int idx) {
	int i;
	printf("%*d) ", indent, idx);
	switch(data->c) {
		case '*':
			printf("*%d\n", data->sz);
			indent += ceil(log10(data->sz));
			for(i=0; i<data->sz; i++) _print_data(&data->data[i], indent+2, i+1);
			break;
		case '$':
			printf("$%d %s\n", data->sz, data->sz > 0 ? data->str : "");
			break;
		case ':':
			printf(":%ld\n", data->l);
			break;
		default:
			printf("%c%s\n", data->c, data->str ? data->str : "");
			break;
	}
}

#define print_data(data) do { \
		printf("************************************************************************************\n"); \
		_print_data(data, 0, 1); \
	} while(0)

int main(int argc, const char *argv[]) {
	const char *host = "127.0.0.1";
	int port = 6379;
	const char *auth = "";
	int database = 0;
	redis_t redis;
	long int size = -1;
	int opt, _optind;
	int status = EXIT_SUCCESS;
	int flag = 0, i, loop = 1, exists;
	char **keys = NULL;
	char *rtype = NULL;
	char rtype2[32];
	char *rtype2ptr[] = {rtype2};

	while((opt = getopt(argc, (char**) argv, "h:p:a:n:l:vd?")) != -1) {
		switch(opt) {
			case 'h':
				host = optarg;
				break;
			case 'p':
				port = atoi(optarg);
				break;
			case 'a':
				auth = optarg;
				break;
			case 'n':
				database = atoi(optarg);
				break;
			case 'l':
				loop = atoi(optarg);
				break;
			case 'v':
				printf("%s\n", REDIS_VERSION);
				return EXIT_SUCCESS;
			case 'd':
				flag |= REDIS_FLAG_DEBUG;
				break;
			case '?':
				goto usage;
		}
	}

	_optind = optind;
begin:
	optind = _optind;

	if(!redis_init(&redis, flag)) return EXIT_FAILURE;

	printf("Connecting to %s:%d\n", host, port);
	if(!redis_connect(&redis, host, port)) goto end;
	printf("Connected  to %s:%d\n", redis.ip, port);

	if(*auth && !redis_auth(&redis, auth)) goto end;
	if(!redis_ping(&redis)) goto end;
	if(!redis_echo(&redis, "Hello World!!!")) goto end;
	if(!redis_select(&redis, database)) goto end;
	if(!redis_dbsize(&redis, &size)) goto end;
	printf("************************************************************************************\n");
	printf("DBSIZE: %ld\n", size);
	if(!redis_keys(&redis, "*", &keys)) goto end;
	if(keys) {
		printf("************************************************************************************\n");
		for(i=0; keys[i]; i++) {
			printf("KEYS(%d): %s\n", i+1, keys[i]);
		}
		free(keys);
		keys = NULL;
	}

	{
		time_t t = time(NULL);
		struct tm *tm = localtime(&t);
		strftime(rtype2, sizeof(rtype2), "%F %T", tm);
		if(!redis_set(&redis, "test", rtype2)) goto end;
		if(!redis_get(&redis, "test", &rtype)) goto end;
		if(rtype) {
			printf("************************************************************************************\n");
			printf("TIME: %s\n", rtype);
			free(rtype);
			rtype = NULL;
		}
	}

	if(!redis_type(&redis, "test", &rtype)) goto end;
	if(rtype) {
		printf("************************************************************************************\n");
		printf("TYPE: %s\n", rtype);
		free(rtype);
	}

	if(!redis_type(&redis, "test", rtype2ptr)) goto end;
	printf("************************************************************************************\n");
	printf("TYPE(nofree): %s\n", rtype2);

	size = 0;
	if(!redis_multi(&redis)) goto end;
	if(!redis_dbsize(&redis, NULL)) goto end;
	if(!redis_keys(&redis, "*", NULL)) goto end;
	if(!redis_type(&redis, "test", NULL)) goto end;
	if(!redis_get(&redis, "test2", NULL)) goto end;
	if(!redis_exec(&redis)) goto end;
	print_data(&redis.data);

	if(!redis_scan(&redis, 0, "*", 10)) goto end;
	print_data(&redis.data);
	
	{
		const char *argv[] = {"test3", "value3", "test4", "", "test5", "value5"};
		const int argc = sizeof(argv)/sizeof(argv[0]);
		const char *keyv[] = {"test3", "test4", "test5"};
		const int keyc = sizeof(keyv)/sizeof(keyv[0]);
		char *value[] = {NULL, NULL, NULL};
		int size[] = {0, 0, 0};

		if(!redis_mset(&redis, argc, argv)) goto end;
		if(!redis_append(&redis, "test3", "-append")) goto end;
		if(redis.data.c == REDIS_FLAG_INT) {
			printf("************************************************************************************\n");
			printf("redis_append: %ld\n", redis.data.l);
		}
		if(!redis_mget_ex(&redis, keyc, keyv, value, size)) goto end;
		
		if(redis.data.c == REDIS_FLAG_MULTI) {
			printf("************************************************************************************\n");
			for(i=0; i<keyc; i++) {
				printf(" %d) $%d %s\n", i, size[i], value[i] ? value[i] : NULL);
				if(value[i]) {
					free(value[i]);
					value[i] = NULL;
				}
			}
		}
		
		if(!redis_del_ex(&redis, keyc, keyv, &exists)) goto end;
		printf("************************************************************************************\n");
		printf("redis_del_ex: %d\n", exists);
	}
	
	if(!redis_del_keys(&redis, "*", &exists)) goto end;
	printf("************************************************************************************\n");
	printf("redis_del_keys: %d\n", exists);

	if(optind < argc) {
		for(i=optind; i<argc; i++) {
			opt = strcmp(argv[i], ";");
			if(i+1 == argc || !opt) {
				if(!redis_senda(&redis, i - optind + (!opt ? 0 : 1), &argv[optind])) goto end;
				if(!redis_recv(&redis, REDIS_FLAG_ANY)) goto end;
				print_data(&redis.data);
				optind = i+1;
			}
		}
	}

	if(!redis_quit(&redis)) goto end;

end:
	redis_close(&redis);
	redis_destory(&redis);

	if((--loop) > 0) goto begin;

	return 0;

usage:
	fprintf(stderr,
		"Usage: %s [options] [--] [cmd [arg [arg ...]]] [\\; cmd2 [arg2 [arg2...]]] ... \n"
		"    options\n"
		"        -h <host>            Server hostname (value: %s)\n"
		"        -p <port>            Server port (value: %d)\n"
		"        -a <password>        Password to use when connecting to the server(value: %s)\n"
		"        -n <database>        Database number(value: %d)\n"
		"        -l <loop>            Loop times(value: %d)\n"
		"        -d                   Open network debug info\n"
		"        -v                   Output version and exit\n"
		"        -?                   Output this help and exit\n"
		, argv[0], host, port, auth, database, loop);

	return status;
}
