#include <sys/time.h>
#include <stdlib.h>
#include "timer.h"


long get_time_in_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}