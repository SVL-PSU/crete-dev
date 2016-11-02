#ifndef CRETE_HOOK_UTILITY_H
#define CRETE_HOOK_UTILITY_H

#include <stddef.h> // For size_t

/**
 * @brief crete_symfile_memcpy - version of memcpy meant to exist in the address space
 *        of libcrete_hook.so, so as to be always captured. Will be used until incremental
 *        capture is proficient enough to capture memcpy without issue.
 * @param dst
 * @param src
 * @param size
 */
void crete_symfile_memcpy(void* dst, const void* src, size_t size);

#endif // CRETE_HOOK_UTILITY_H
