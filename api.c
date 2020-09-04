#include "api.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#define MICRO_IN_SEC 1000000.00

double microtime()
{
	struct timeval tp = {0};

	if (gettimeofday(&tp, NULL)) {
		return 0;
	}

	return (double)(tp.tv_sec + tp.tv_usec / MICRO_IN_SEC);
}

char *fsize(int size)
{
	char units[5][3]={"B","KB","MB","GB","TB"};
	char buf[10];
	int unit=(int)(log(size)/log(1024));

	if (unit>4)
	{
		unit=4;
	}

	sprintf(buf, "%.3f%s", size/pow(1024,unit), units[unit]);

	return strdup(buf);
}
