#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "redis.h"

int main(int argc, const char *argv[]) {
	const char *host = "127.0.0.1";
	int port = 6379;
	const char *auth = "";
	int database = 0;
	redis_t redis;
	int size = -1;
	int opt;
	int status = EXIT_SUCCESS;
	int flag = 0, i;
	char **keys = NULL;
	char *rtype = NULL;

	while((opt = getopt(argc, (char**) argv, "h:p:a:n:vd?")) != -1) {
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

	if(!redis_type(&redis, "test", &rtype)) goto end;
	if(rtype) {
		printf("************************************************************************************\n");
		printf("TYPE: %s\n", rtype);
		free(rtype);
	}

	if(optind < argc) {
		flag = redis.flag;
		if(!redis_debug(&redis)) goto end;
		if(!redis_senda(&redis, argc - optind, &argv[optind])) goto end;
		if(!redis_recv(&redis, REDIS_FLAG_ANY)) goto end;
		redis.flag = flag;
	}

	if(!redis_quit(&redis)) goto end;

end:
	redis_close(&redis);
	redis_destory(&redis);

	return 0;

usage:
	fprintf(stderr,
		"Usage: %s [options] [--] [cmd [arg [arg ...]]]\n"
		"    options\n"
		"        -h <host>            Server hostname (value: %s)\n"
		"        -p <port>            Server port (value: %d)\n"
		"        -a <password>        Password to use when connecting to the server(value: %s)\n"
		"        -n <database>        Database number(value: %d)\n"
		"        -d                   Open network debug info\n"
		"        -v                   Output version and exit\n"
		"        -?                   Output this help and exit\n"
		, argv[0], host, port, auth, database);

	return status;
}
