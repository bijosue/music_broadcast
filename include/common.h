#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>

/***************************
 * return second since system booted
 * *************************/
uint64_t getUpTimeSec(void);

uint64_t getUpTimemSec(void);

#endif