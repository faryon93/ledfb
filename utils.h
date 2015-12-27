#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdint.h>
#include <sys/time.h>

uint64_t clock_us(void)
{
	struct timeval time;
	gettimeofday(&time, NULL);

	return (time.tv_sec * 1000 * 1000) + time.tv_usec;
}

#endif