#pragma once
#include <time.h>
#ifdef __cplusplus
extern "C" {
#endif
int sceKernelUsleep(unsigned int microseconds);
static inline int nanosleep(const struct timespec* req, struct timespec* rem) {
    (void)rem;
    sceKernelUsleep((unsigned)(req->tv_sec * 1000000 + req->tv_nsec / 1000));
    return 0;
}
#ifdef __cplusplus
}
#endif
