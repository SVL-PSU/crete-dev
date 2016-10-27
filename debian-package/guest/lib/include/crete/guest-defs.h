#ifndef GUEST_DEFS
#define GUEST_DEFS

#ifndef __STDC_FORMAT_MACROS // Required for PRIu64, etc.
    #define __STDC_FORMAT_MACROS
#endif // __STDC_FORMAT_MACROS

#include <stdint.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#ifdef __x86_64__ // 64bit architecture
    #define PRIu PRIu64
#elif defined __i386__ // 32bit architecture
    #define PRIu PRIu32
#endif // uint_t

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // GUEST_DEFS
