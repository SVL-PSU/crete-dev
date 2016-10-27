#include <crete/hook/utility.h>

// TODO: (low) copying byte-by-byte is inefficient.
void crete_symfile_memcpy(void* dst, const void* src, size_t size)
{
    char* d = (char*)dst;
    char* s = (char*)src;
    size_t i;
    for(i = 0; i < size; ++i)
        d[i] = s[i];
}
