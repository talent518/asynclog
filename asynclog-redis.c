#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "redis.h"

void print_multi(multi_redis_t *multi, int size) {
	int i, opt;
	printf("************************************************************************************\n");
	for(i=0; i<size; i++) {
		switch(multi[i].c) {
			case '*':
				for(opt=0; opt<multi[i].argc; opt++) {
					printf("ARR[%d]: ", opt);
					if(multi[i].argv[opt].str) {
						fwrite(multi[i].argv[opt].str, 1, multi[i].argv[opt].len, stdout);
						free(multi[i].argv[opt].str);
					}
					printf("\n");
				}
				if(multi[i].argv) free(multi[i].argv);
				break;
			case '$':
				printf("STRING: ");
				if(multi[i].argc && multi[i].argv[0].str) {
					fwrite(multi[i].argv[0].str, 1, multi[i].argv[0].len, stdout);
					free(multi[i].argv[0].str);
				}
				printf("\n");
				if(multi[i].argv) free(multi[i].argv);
				break;
			default:
				printf("STATUS: %s\n", multi[i].buf);
		}
	}
	free(multi);
}

int main(int argc, const char *argv[]) {
	const char *host = "127.0.0.1";
	int port = 6379;
	const char *auth = "";
	int database = 0;
	redis_t redis;
	int size = -1;
	int opt, _optind;
	int status = EXIT_SUCCESS;
	int flag = 0, i, loop = 1;
	char **keys = NULL;
	char *rtype = NULL;
	char rtype2[32];
	char *rtype2ptr[] = {rtype2};
	multi_redis_t *multi = NULL;

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
	printf("DBSIZE: %d\n", size);
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
	if(!redis_exec(&redis, &multi, &size)) goto end;
	if(multi && size) {
		print_multi(multi, size);
		multi = NULL;
		size = 0;
	}

	if(!redis_scan(&redis, 0, "*", 10, &multi, &size)) goto end;
	if(multi && size) {
		print_multi(multi, size);
		multi = NULL;
		size = 0;
	}

	if(optind < argc) {
		flag = redis.flag;
		if(!redis_debug(&redis)) goto end;
		for(i=optind; i<argc; i++) {
			opt = strcmp(argv[i], ";");
			if(i+1 == argc || !opt) {
				if(strcasecmp(argv[optind], "exec")) {
					if(!redis_senda(&redis, i - optind + (!opt ? 0 : 1), &argv[optind])) goto end;
					if(!redis_recv(&redis, REDIS_FLAG_ANY)) goto end;
				} else {
					if(!redis_exec(&redis, NULL, NULL)) goto end;
				}
				optind = i+1;
			}
		}
		redis.flag = flag;
	}

	if(!redis_quit(&redis)) goto end;

end:
	redis_close(&redis);
	redis_destory(&redis);

	if((--loop) > 0) goto begin;

	return 0;

usage:
	fprintf(stderr,
		"Usage: %s [options] [--] [cmd [arg [arg ...]]]\n"
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
