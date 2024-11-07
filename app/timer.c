#include <sys/time.h>
#include <stdlib.h>
#include "timer.h"


unsigned long get_time_in_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long)tv.tv_sec * 1000 + (unsigned long)tv.tv_usec / 1000;
}