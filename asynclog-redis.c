#include <stdio.h>
#include <stdlib.h>

#include "redis.h"

int main(int argc, const char *argv[]) {
	const char *host = "127.0.0.1";
	int port = 6379;
	const char *auth = NULL;
	int database = 0;
	redis_t redis;
	int size = -1;

	if(argc >= 2) host = argv[1];
	if(argc >= 3) port = atoi(argv[2]);
	if(argc >= 4) auth = argv[3];
	if(argc >= 5) database = atoi(argv[4]);

	if(!redis_init(&redis, 0)) return 1;

	printf("Connecting to %s:%d\n", host, port);
	if(!redis_connect(&redis, host, port)) goto end;
	printf("Connected  to %s:%d\n", redis.ip, port);

	if(auth && *auth && !redis_auth(&redis, auth)) goto end;
	if(!redis_ping(&redis)) goto end;
	if(!redis_echo(&redis, "Hello World!!!")) goto end;
	if(!redis_select(&redis, database)) goto end;
	if(!redis_dbsize(&redis, &size)) goto end;
	if(!redis_keys(&redis, "*", NULL)) goto end;

	if(!redis_type(&redis, "test")) goto end;

	if(!redis_quit(&redis)) goto end;

end:
	redis_close(&redis);
	redis_destory(&redis);

	return 0;
}
