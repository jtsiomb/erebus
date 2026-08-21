#include <time.h>
#include <sys/time.h>
#include "erebus.h"

struct scene scn;

unsigned long get_msec(void)
{
	static struct timeval tv0;
	struct timeval tv;

	gettimeofday(&tv, 0);
	if(!(tv0.tv_usec | tv0.tv_sec)) {
		tv0 = tv;
		return 0;
	}
	return (tv.tv_sec - tv0.tv_sec) * 1000 + (tv.tv_usec - tv0.tv_usec) / 1000;
}
