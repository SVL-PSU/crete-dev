#ifndef CRETE_HOST_HARNESS
#define CRETE_HOST_HARNESS

// TODO: why is this not a symbolic link to guest/include/crete/harness.h?

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void crete_initialize(int argc, char* argv[]);
int crete_start(int (*harness)(int argc, char* argv[]));
void crete_make_concolic(void* addr, size_t size, const char* name);
void crete_make_concolic_file(const char* name, size_t size);
void crete_make_concolic_file_input(const char* name, size_t size, const uint8_t* input);
void crete_assume(int cond);
void crete_initialize_concolic_posix_files(size_t file_count);
void crete_make_concolic_file_posix(const char* name, size_t size);
int crete_posix_is_symbolic_fd(int fd);

// TODO: Temporarily provided here. This is part of the workaround for guest/include/harness.h. Does nothing.
void __crete_make_symbolic();

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // CRETE_HOST_HARNESS

