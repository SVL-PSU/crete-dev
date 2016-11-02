#ifndef CRETE_HOOK_H
#define CRETE_HOOK_H

#include <crete/hook/utility.h>

#ifdef __cplusplus
extern "C" {
#else
    #include <stdbool.h>
#endif // __cplusplus

#include <stdint.h>

#define CRETE_MAX_FILENAME_SIZE 256 // ~Standard.

enum FileMode
{
    FILE_MODE_NONE = 0,
    FILE_MODE_READ,
    FILE_MODE_WRITE,
    FILE_MODE_APPEND,
    FILE_MODE_RW,
    FILE_MODE_RW_TRUNC,
    FILE_MODE_APPEND_RW
};

enum FileOp
{
    FILE_OP_NONE = 0,
    FILE_OP_READ,
    FILE_OP_WRITE
};

typedef struct CreteConcolicFile_s
{
    size_t size;
    uint8_t* data;
    char* name;

} CreteConcolicFile;

typedef struct CRETE_FILE_s
{
//    char name[CRETE_MAX_FILENAME_SIZE];
    uint32_t concolic_file_index; // Index to file array. Initialize to UINT_MAX.
    uint32_t concolic_file_handle_index; // Index to file handle array. Initialize to UINT_MAX.
    size_t size;
    uint8_t* contents;
    uint8_t* stream_pos;
    uint8_t* stream_end; // TODO: I think this misconceptualized. It makes more sense to have it point to one-past-the-end, as c++ iterators do. This way, "remaining_bytes_to_read = stream_end - stream_begin;" will return the expected value.
    bool is_open;
    bool eof;
    int pushed_back; // == CRETE_EOF when no bytes are pushed back. Of type int to allow for EOF(-1).
    enum FileMode mode;
    enum FileOp last_op;
} CRETE_FILE;

CreteConcolicFile* crete_make_blank_file();
CRETE_FILE* crete_make_blank_concolic_stdin(void);
void crete_cleanup_concolic_files();

void crete_make_concolic_file(const char* name, size_t size); // TODO: why is this declared here?
void crete_make_concolic_file_input(const char* name,
                                    size_t size,
                                    const uint8_t* input); // TODO: why is this declared here?

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // CRETE_HOOK_H
