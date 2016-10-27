#ifndef CRETE_GUEST_H
#define CRETE_GUEST_H

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void crete_initialize(int argc, char* argv[]);
int crete_start(int (*harness)(int argc, char* argv[]));

void crete_make_concolic(void* addr, size_t size, const char* name);
void crete_assume_begin();
void crete_assume_(int cond);

#define crete_assume(cond) \
    crete_assume_begin(); \
    crete_assume_(cond)

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // CRETE_GUEST_H
