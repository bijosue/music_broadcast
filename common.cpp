#include "include/common.h"

uint64_t getUpTimeSec(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) >= 0)
    {
        return ts.tv_sec;
    }
    return 0;
}
uint64_t getUpTimemSec(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) >= 0)
    {
        return ts.tv_sec*1000+ts.tv_nsec/1000;
    }
    return 0;
}