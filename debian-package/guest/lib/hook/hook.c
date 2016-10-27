#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <dlfcn.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <fcntl.h>

#include <crete/hook/hook.h>
#include "klee/fd.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define CRETE_EOF -1

#define CRETE_MAX_CONCOLIC_FILE_ENTRIES 8 // Max number of concolic files. Arbitrarily chosen.
#define CRETE_MAX_CONCOLIC_FILE_HANDLE_ENTRIES 16 // Max number of concolic file handles. Somewhat arbitrarily chosen (2*CRETE_MAX_CONCOLIC_FILE_ENTRIES).

struct CreteConcolicFiles
{
    size_t size;
    CreteConcolicFile entries[CRETE_MAX_CONCOLIC_FILE_ENTRIES];

} crete_concolic_files;

struct CreteConcolicFileHandles
{
   size_t size;
   CRETE_FILE entries[CRETE_MAX_CONCOLIC_FILE_HANDLE_ENTRIES];
   int entry_is_in_use[CRETE_MAX_CONCOLIC_FILE_HANDLE_ENTRIES];

} crete_concolic_file_handles;

CRETE_FILE crete_stdin_concolic_handle_v = {0};
CRETE_FILE* crete_stdin_concolic_handle = &crete_stdin_concolic_handle_v;
extern void* stdin;

// Begin Prototypes
bool crete_symfile_has_pushed_back(CRETE_FILE* stream);
uint8_t crete_symfile_unget_pushed_back(CRETE_FILE* stream);
CRETE_FILE* crete_get_associated_stream(CRETE_FILE* stream);

long ftell(CRETE_FILE *stream);
// End Prototypes

bool crete_symfile_is_stream_concolic(CRETE_FILE* stream)
{
//    assert(crete_concolic_file_handles.size > 0); Can't have this on account of the possibility that this is checked before any concolic files are made (non-concolic file operations).

    if(stream == crete_stdin_concolic_handle)
    {
        return true;
    }

    return
        (stream >= &crete_concolic_file_handles.entries[0] &&
         stream <= &crete_concolic_file_handles.entries[crete_concolic_file_handles.size - 1]);
}

void* crete_dlsym_next(const char* name)
{
    void* func = dlsym(RTLD_NEXT, name);

    assert(func && "[CRETE Error] libcrete_hook.so - failed to load shared library function");

    return func;
}

CRETE_FILE* crete_get_associated_stream(CRETE_FILE* stream)
{
    if(stream == stdin)
    {
        return crete_stdin_concolic_handle;
    }

    // If not stdin, just return the stream itself.
    return stream;
}

CRETE_FILE* crete_symfile_open_concolic_file(const char* filename)
{
    uint32_t concolic_file_index = UINT_MAX;
    uint32_t concolic_file_handle_index = UINT_MAX;
    CreteConcolicFile* file = NULL;
    CRETE_FILE* stream_handle = NULL;

    size_t i;
    for(i = 0; i < crete_concolic_files.size; ++i)
    {
        if(strcmp(filename, crete_concolic_files.entries[i].name) == 0)
        {
            concolic_file_index = i;
            break;
        }
    }

    if(concolic_file_index == UINT_MAX)
        return NULL;

    assert(crete_concolic_file_handles.size + 1 < CRETE_MAX_CONCOLIC_FILE_HANDLE_ENTRIES);

    for(i = 0; i < CRETE_MAX_CONCOLIC_FILE_HANDLE_ENTRIES; ++i)
    {
        if(crete_concolic_file_handles.entry_is_in_use[i] == 0)
        {
            concolic_file_handle_index = i;
            crete_concolic_file_handles.entry_is_in_use[i] = 1;
            ++crete_concolic_file_handles.size;
            break;
        }
    }

    file = &crete_concolic_files.entries[concolic_file_index];
    stream_handle = &crete_concolic_file_handles.entries[concolic_file_handle_index];

    stream_handle->contents = malloc(file->size);
    assert(stream_handle->contents);
    // Use local memcpy to ensure it's captured.
    crete_symfile_memcpy(stream_handle->contents, file->data, file->size);

    stream_handle->size = file->size;
    stream_handle->concolic_file_index = concolic_file_index;
    stream_handle->concolic_file_handle_index = concolic_file_handle_index;
    stream_handle->stream_pos = &stream_handle->contents[0];
    stream_handle->stream_end = &stream_handle->contents[stream_handle->size - 1];
    stream_handle->last_op = FILE_OP_NONE;
    stream_handle->mode = FILE_MODE_NONE;
    stream_handle->is_open = true;
    stream_handle->eof = false;
    stream_handle->pushed_back = CRETE_EOF;

    return stream_handle;
}

void crete_symfile_reset(CRETE_FILE* stream)
{
    if(stream->contents)
    {
        free(stream->contents);
    }

    stream->contents = NULL;
    stream->size = 0;
    stream->is_open = false;
    stream->eof = false;
    stream->contents = NULL;
    stream->stream_pos = NULL;
    stream->stream_end = NULL;
    stream->concolic_file_index = UINT_MAX;
    stream->concolic_file_handle_index = UINT_MAX;
    stream->last_op = FILE_OP_NONE;
    stream->mode = FILE_MODE_NONE;
}

int crete_symfile_close_concolic_file(CRETE_FILE* stream)
{
    if(!stream->is_open)
    {
        errno = EBADF;
        return CRETE_EOF;
    }

    assert(stream->concolic_file_handle_index != UINT_MAX);

    crete_concolic_file_handles.entry_is_in_use[stream->concolic_file_handle_index] = 0;

    assert(stream->contents && stream->size);

    crete_symfile_reset(stream);

    assert(crete_concolic_file_handles.size > 0);

    --crete_concolic_file_handles.size;

    return 0;
}

bool crete_set_up_mode(CRETE_FILE* stream, const char* mode)
{
    switch(mode[0])
    {
    case 'r':
        if(mode[1] == '+')
            stream->mode = FILE_MODE_RW;
        else
            stream->mode = FILE_MODE_READ;
        break;
    case 'w':
        if(mode[1] == '+')
            stream->mode = FILE_MODE_RW_TRUNC;
        else
            stream->mode = FILE_MODE_WRITE;
        break;
    case 'a':
        if(mode[1] == '+')
            stream->mode = FILE_MODE_APPEND_RW;
        else
            stream->mode = FILE_MODE_APPEND;
        break;
    default:
        return false;
    }

    return true;
}

CreteConcolicFile* crete_make_blank_file()
{
    assert((crete_concolic_files.size + 1) < CRETE_MAX_CONCOLIC_FILE_ENTRIES);

    CreteConcolicFile* file = &crete_concolic_files.entries[crete_concolic_files.size++];

    file->data = NULL;
    file->name = NULL;
    file->size = 0;

    return file;
}

void crete_cleanup_concolic_files()
{
    size_t i;

    for(i = 0; i < crete_concolic_files.size; ++i)
    {
        CreteConcolicFile* file = &crete_concolic_files.entries[i];

        if(file->data)
            free(file->data);
        if(file->name)
            free(file->name);
    }
}

bool crete_symfile_is_stream_open(CRETE_FILE* stream)
{
    return stream->is_open;
}

bool crete_symfile_is_valid_read_stream(CRETE_FILE* stream)
{
    if(!crete_symfile_is_stream_open(stream) &&
       !(stream->mode == FILE_MODE_READ ||
         stream->mode == FILE_MODE_RW ||
         stream->mode == FILE_MODE_RW_TRUNC ||
         stream->mode == FILE_MODE_APPEND_RW))
    {
        return false;
    }

    return true;
}

// Return can't be uint8_t, as EOF == -1
int crete_symfile_read_byte(CRETE_FILE* stream)
{
    // TODO: I don't think this is correct. That is, fgetc is suppose to be able to get incorrect behavior in a loop while checking the eof flag. With this impl., we erroneously get correct behavior.
    if(stream->eof || stream->stream_pos == stream->stream_end)
    {
        stream->eof = true;
        return CRETE_EOF;
    }

    if(crete_symfile_has_pushed_back(stream))
    {
        return crete_symfile_unget_pushed_back(stream);
    }

    uint8_t ret = *stream->stream_pos;
    ++stream->stream_pos;

    return ret;
}

ptrdiff_t crete_symfile_remaining_read_bytes(CRETE_FILE* stream)
{
    return stream->stream_end - stream->stream_pos + 1; // See note in header for stream_end.
}

bool crete_symfile_has_pushed_back(CRETE_FILE* stream)
{
    return stream->pushed_back != CRETE_EOF;
}

uint8_t crete_symfile_unget_pushed_back(CRETE_FILE* stream)
{
    uint8_t c = stream->pushed_back;

    stream->pushed_back = CRETE_EOF;

    return c;
}

// ************* C File Routines *******************

typedef void* (*fopen_ty)(const char*, const char*);
CRETE_FILE* fopen(const char* filename, const char* mode)
{
    CRETE_FILE* stream = crete_symfile_open_concolic_file(filename);

    if(stream == NULL)
    {
        fopen_ty fn = (fopen_ty)crete_dlsym_next("fopen");
        return fn(filename, mode);
    }

    if(!crete_set_up_mode(stream, mode))
    {
        crete_symfile_close_concolic_file(stream);
        errno = EINVAL;
        return NULL;
    }

    return stream;
}

// TODO: file size differences are not accounted for between fopen,fopen64 (low priority). 2Gb max of fopen likely won't be an issue for the immediate future.
// Note: fopen64 is non-standard, so I'm even less concerned about distinguishing between fopen and fopen64.
typedef void* (*fopen64_ty)(const char*, const char*);
CRETE_FILE* fopen64(const char* filename, const char* mode)
{
    return fopen(filename, mode);
}

typedef int (*fclose_ty)(void*);
int fclose(CRETE_FILE* stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        fclose_ty fn = (fclose_ty)crete_dlsym_next("fclose");
        return fn(stream);
    }

    if(stream == crete_stdin_concolic_handle)
    {
        crete_symfile_reset(stream);

        return 0;
    }

    return crete_symfile_close_concolic_file(stream);
}

typedef size_t (*fread_ty)(void*, size_t, size_t, void*);
size_t fread(void* ptr, size_t size, size_t nitems, CRETE_FILE* stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        fread_ty fn = (fread_ty)crete_dlsym_next("fread");
        return fn(ptr, size, nitems, stream);
    }

    uint8_t* bptr = (uint8_t*)ptr;

    if(!crete_symfile_is_valid_read_stream(stream))
    {
        errno = EBADF;
        return CRETE_EOF;
    }

    ptrdiff_t items_size = size * nitems;

    if(items_size == 0)
    {
        return 0;
    }

    ptrdiff_t remaining_bytes = crete_symfile_remaining_read_bytes(stream);

    ptrdiff_t read_size = items_size < remaining_bytes ? items_size : remaining_bytes;

    if(crete_symfile_has_pushed_back(stream))
    {
        bptr[0] = crete_symfile_unget_pushed_back(stream);
        --read_size;
        ++bptr;
    }

    // Conversely, I could repeatedly call fgetc (as specified in the spec. but the effect is the same,
    // and this is more efficient).
    // Need local non-libc memcpy, so it's always captured.
    crete_symfile_memcpy(bptr, stream->stream_pos, read_size);
    stream->stream_pos += read_size;

    if(stream->stream_pos == stream->stream_end)
    {
        stream->eof = true;
    }

    // That less than nitems may be returned shall occurr in an error or EOF.
    // Otherwise, this should return the equivalent of nitems.
    // Also, this means "partial elements/items" may be returned in case of EOF.
    return read_size / size;
}

typedef void (*clearerr_ty)(void*);
void clearerr(CRETE_FILE *stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        clearerr_ty fn = (clearerr_ty)crete_dlsym_next("clearerr");
        fn(stream);
        return;
    }

    assert(0 && "CRETE: unimplemented file routine called: clearerr");
    return;
}

/**
 * Notes:
 * -EOF is a flag. It is not a check on stream positions. It is set after another operation -not by this function.
 */
typedef int (*feof_ty)(void*);
int feof(CRETE_FILE* stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        feof_ty fn = (feof_ty)crete_dlsym_next("feof");
        return fn(stream);
    }

    return stream->eof;
}

typedef int (*ferror_ty)(void*);
int ferror(CRETE_FILE* stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        ferror_ty fn = (ferror_ty)crete_dlsym_next("ferror");
        return fn(stream);
    }

    assert(0 && "CRETE: unimplemented file routine called: ferror");
    return 0;
}

typedef int (*fflush_ty)(void*);
int fflush(CRETE_FILE* stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        fflush_ty fn = (fflush_ty)crete_dlsym_next("fflush");
        return fn(stream);
    }

    assert(0 && "CRETE: unimplemented file routine called: fflush");
    return 0;
}

typedef int (*fgetc_ty)(void*);
int fgetc(CRETE_FILE* stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        fgetc_ty fn = (fgetc_ty)crete_dlsym_next("fgetc");
        return fn(stream);
    }

    if(!crete_symfile_is_valid_read_stream(stream))
    {
        errno = EBADF;
        return CRETE_EOF;
    }

    int ret = crete_symfile_read_byte(stream);

    return ret;
}

typedef int (*fgetpos_ty)(void*, void*);
int fgetpos(CRETE_FILE * stream, void * pos)
{    
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        fgetpos_ty fn = (fgetpos_ty)crete_dlsym_next("fgetpos");
        return fn(stream, pos);
    }

    assert(0 && "CRETE: unimplemented file routine called: fgetpos");
    return 0;
}

typedef char* (*fgets_ty)(char*, int, void*);
char * fgets(char * s, int n, CRETE_FILE * stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        fgets_ty fn = (fgets_ty)crete_dlsym_next("fgets");
        return fn(s, n, stream);
    }

    if(!crete_symfile_is_valid_read_stream(stream))
    {
        errno = EBADF;
        return NULL;
    }

    if(n <= 1)
    {
        s[0] = '\0';
        return s;
    }

    ptrdiff_t bytes_to_get = n - 1;

    if(stream->eof)
    {
        s[0] = '\0';
        return NULL;
    }

    ptrdiff_t remaining_bytes = crete_symfile_remaining_read_bytes(stream);

    ptrdiff_t read_size = bytes_to_get < remaining_bytes ? bytes_to_get : remaining_bytes;

    if(crete_symfile_has_pushed_back(stream))
    {
        s[0] = crete_symfile_unget_pushed_back(stream);
        --read_size;
        ++s;
    }

    int i;
    for(i = 0; i < read_size;)
    {
        char c = *stream->stream_pos;

        s[i] = *stream->stream_pos;
        ++stream->stream_pos;

        ++i;

        if(c == '\n')
            break;
    }
    s[i] = '\0';

    // TODO: this isn't right (I don't think).
    // If the size of the file is N and N bytes are read, NULL should NOT be returned.
    if(stream->stream_pos == stream->stream_end)
    {
        stream->eof = true;
        return NULL;
    }

    return s;
}

typedef int (*fileno_ty)(void*);
int fileno(CRETE_FILE *stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        fileno_ty fn = (fileno_ty)crete_dlsym_next("fileno");

        int sys_fd = fn(stream);

        int fd = crete_existing_fd(sys_fd);
        if(fd != -1)
        {
            return fd;
        }

        fd = crete_next_available_fd();

        assert(fd != -1 && "[CRETE] file descriptors exhausted");

        exe_file_t *f = &__exe_env.fds[fd];

        // Since it's concrete, just give all params and let system impl. deal with it.
        f->flags = eOpen | eReadable | eWriteable;
        f->dfile = 0;
        f->fd = sys_fd;
        f->off = lseek(fd, 0, SEEK_CUR);

        return fd;
    }

    assert(0 && "CRETE: unimplemented file routine called: fileno");
    return 0;
}

typedef void (*flockfile_ty)(void*);
void flockfile(CRETE_FILE *file)
{
    file = crete_get_associated_stream(file);

    if(!crete_symfile_is_stream_concolic(file))
    {
        flockfile_ty fn = (flockfile_ty)crete_dlsym_next("flockfile");
        fn(file);
        return;
    }

    assert(0 && "CRETE: unsupported threading file routine called: flockfile");
    return;
}

typedef int (*ftrylockfile_ty)(void*);
int ftrylockfile(CRETE_FILE *file)
{
    file = crete_get_associated_stream(file);

    if(!crete_symfile_is_stream_concolic(file))
    {
        ftrylockfile_ty fn = (ftrylockfile_ty)crete_dlsym_next("ftrylockfile");
        return fn(file);
    }

    assert(0 && "CRETE: unsupported threading file routine called: ftrylockfile");
    return 0;
}

typedef void (*funlockfile_ty)(void*);
void funlockfile(CRETE_FILE *file)
{
    file = crete_get_associated_stream(file);

    if(!crete_symfile_is_stream_concolic(file))
    {
        funlockfile_ty fn = (funlockfile_ty)crete_dlsym_next("funlockfile");
        fn(file);
        return;
    }

    assert(0 && "CRETE: unsupported threading file routine called: funlockfile");
    return;
}

//CRETE_FILE *fmemopen(void * buf, size_t size,
//       const char * mode)
//{
//    assert(0 && "CRETE: unimplemented file routine called: fmemopen");
//    return 0;
//}

typedef int (*fvprintf_ty)(void*, const char*, va_list);
int vfprintf(CRETE_FILE * stream, const char * format,
       va_list ap)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        fvprintf_ty fn = (fvprintf_ty)crete_dlsym_next("vfprintf");
        return fn(stream, format, ap);
    }

    assert(0 && "CRETE: unimplemented file routine called: vfprintf");
    return 0;
}

/**
 * Notes:
 *  Inspiration from: http://c-faq.com/varargs/handoff.html
 *  I use the equivalent vfprintf functions internally in order to hand off the variadic argument.
 **/
int fprintf(CRETE_FILE * stream, const char * format, ...)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        va_list argp;
        va_start(argp, format);

        int ret = vfprintf(stream, format, argp);

        va_end(argp);

        return ret;
    }

    assert(0 && "CRETE: unimplemented file routine called: fprintf");
    return 0;
}

typedef int (*fputc_ty)(int, void*);
int fputc(int c, CRETE_FILE *stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        fputc_ty fn = (fputc_ty)crete_dlsym_next("fputc");
        return fn(c, stream);
    }

    assert(0 && "CRETE: unimplemented file routine called: fputc");
    return 0;
}

typedef int (*fputs_ty)(const char*, void*);
int fputs(const char * s, CRETE_FILE * stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        fputs_ty fn = (fputs_ty)crete_dlsym_next("fputs");
        return fn(s, stream);
    }

    assert(0 && "CRETE: unimplemented file routine called: fputs");
    return 0;
}

typedef void* (*freopen_ty)(const char*, const char*, void*);
CRETE_FILE *freopen(const char * pathname,
                    const char * mode,
                    CRETE_FILE * stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        freopen_ty fn = (freopen_ty)crete_dlsym_next("freopen");
        return fn(pathname, mode, stream);
    }

    if(stream == crete_stdin_concolic_handle)
    {
        CRETE_FILE* tmp = fopen(pathname, mode);

        if(tmp == NULL)
            return NULL;

        fclose(crete_stdin_concolic_handle);

        crete_stdin_concolic_handle = tmp; // Re-associate the stdin_handle to the disk file handle.

        return stream;
    }

    assert(0 && "CRETE: unimplemented file routine called: freopen");
    return 0;
}

// TODO: see comments on fopen64.
typedef void* (*freopen64_ty)(const char*, const char*, void*);
CRETE_FILE *freopen64(const char * pathname, const char * mode,
       CRETE_FILE * stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        freopen_ty fn = (freopen_ty)crete_dlsym_next("freopen64");
        return fn(pathname, mode, stream);
    }

    return freopen(pathname, mode, stream);
}

typedef int (*vfscanf_ty)(void*, const char*, va_list);
int vfscanf(CRETE_FILE * stream, const char * format,
       va_list arg)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        vfscanf_ty fn = (vfscanf_ty)crete_dlsym_next("vfscanf");
        return fn(stream, format, arg);
    }

    assert(0 && "CRETE: unimplemented file routine called: vfscanf");
    return 0;
}

/**
 * Notes:
 *  Inspiration from: http://c-faq.com/varargs/handoff.html
 *  I use the equivalent vfprintf functions internally in order to hand off the variadic argument.
 **/
int fscanf(CRETE_FILE * stream, const char * format, ...)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        va_list argp;
        va_start(argp, format);

        int ret = vfscanf(stream, format, argp);

        va_end(argp);

        return ret;
    }

    assert(0 && "CRETE: unimplemented file routine called: fscanf");
    return 0;
}

typedef int (*fseek_ty)(void*, long, int);
int fseek(CRETE_FILE *stream, long offset, int whence)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        fseek_ty fn = (fseek_ty)crete_dlsym_next("fseek");
        return fn(stream, offset, whence);
    }

    assert(0 && "CRETE: unimplemented file routine called: fseek");
    return 0;
}

typedef int (*fseeko_ty)(void*, off_t, int);
int fseeko(CRETE_FILE *stream, off_t offset, int whence)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        fseeko_ty fn = (fseeko_ty)crete_dlsym_next("fseeko");
        return fn(stream, offset, whence);
    }

    assert(0 && "CRETE: unimplemented file routine called: fseeko");
    return 0;
}

typedef int (*fsetpos_ty)(void*, const void*);
int fsetpos(CRETE_FILE *stream, const void *pos)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        fsetpos_ty fn = (fsetpos_ty)crete_dlsym_next("fsetpos");
        return fn(stream, pos);
    }

    assert(0 && "CRETE: unimplemented file routine called: fsetpos");
    return 0;
}

typedef long (*ftell_ty)(void*);
long ftell(CRETE_FILE *stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        ftell_ty fn = (ftell_ty)crete_dlsym_next("ftell");
        return fn(stream);
    }

    assert(0 && "CRETE: unimplemented file routine called: ftell");
    return 0;
}

typedef off_t (*ftello_ty)(void*);
off_t ftello(CRETE_FILE *stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        ftello_ty fn = (ftello_ty)crete_dlsym_next("ftello");
        return fn(stream);
    }

    assert(0 && "CRETE: unimplemented file routine called: ftello");
    return 0;
}

typedef size_t (*fwrite_ty)(const void*, size_t, size_t, void*);
size_t fwrite(const void * ptr, size_t size, size_t nitems,
       CRETE_FILE * stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        fwrite_ty fn = (fwrite_ty)crete_dlsym_next("fwrite");
        return fn(ptr, size, nitems, stream);
    }

    assert(0 && "CRETE: unimplemented file routine called: fwrite");
    return 0;
}

typedef int (*getc_ty)(void*);
int getc(CRETE_FILE *stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        getc_ty fn = (getc_ty)crete_dlsym_next("getc");
        return fn(stream);
    }

    // According to cplusplus.com, getc and fgetc are equivalent (so I can redirect here);
    // however, gets and fgets are NOT equivalent.
    return fgetc(stream);
}

typedef int (*getc_unlocked_ty)(void*);
int getc_unlocked(CRETE_FILE *stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        getc_unlocked_ty fn = (getc_unlocked_ty)crete_dlsym_next("getc_unlocked");
        return fn(stream);
    }

    return getc(stream);
}

typedef int (*putc_unlocked_ty)(int, void*);
int putc_unlocked(int c, CRETE_FILE *stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        putc_unlocked_ty fn = (putc_unlocked_ty)crete_dlsym_next("putc_unlocked");
        return fn(c, stream);
    }

    assert(0 && "CRETE: unsupported threading file routine called: putc_unlocked");
    return 0;
}

typedef ssize_t (*getdelim_ty)(char**, size_t*, int, void*);
ssize_t getdelim(char ** lineptr,
                 size_t * n,
                 int delimiter,
                 CRETE_FILE * stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        getdelim_ty fn = (getdelim_ty)crete_dlsym_next("getdelim");
        return fn(lineptr, n, delimiter, stream);
    }

    assert(0 && "CRETE: unimplemented file routine called: getdelim");
    return 0;
}

typedef ssize_t (*getline_ty)(char**, size_t*, void*);
ssize_t getline(char ** lineptr, size_t * n,
       CRETE_FILE * stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        getline_ty fn = (getline_ty)crete_dlsym_next("getline");
        return fn(lineptr, n, stream);
    }

    getdelim(lineptr,
             n,
             '\n',
             stream);

    return 0;
}

typedef int (*pclose_ty)(void*);
int pclose(CRETE_FILE *stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        pclose_ty fn = (pclose_ty)crete_dlsym_next("pclose");
        return fn(stream);
    }

    assert(0 && "CRETE: unsupported pipe file routine called: pclose");
    return 0;
}

//typedef int (*popen_ty)(const char*, const char*);
//CRETE_FILE *popen(const char *command, const char *mode)
//{
//    if(!crete_symfile_is_stream_concolic(stream))
//    {
//        popen_ty fn = (popen_ty)crete_dlsym_next("popen");
//        return fn(stream);
//    }

//    assert(0 && "CRETE: unsupported pipe file routine called: popen");
//    return 0;
//}

typedef int (*putc_ty)(int, void*);
int putc(int c, CRETE_FILE *stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        putc_ty fn = (putc_ty)crete_dlsym_next("putc");
        return fn(c, stream);
    }

    assert(0 && "CRETE: unimplemented file routine called: putc");
    return 0;
}

typedef void (*rewind_ty)(void*);
void rewind(CRETE_FILE *stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        rewind_ty fn = (rewind_ty)crete_dlsym_next("rewind");
        fn(stream);
        return;
    }

    assert(0 && "CRETE: unimplemented file routine called: rewind");
    return;
}

typedef void (*setbuf_ty)(void*, char*);
void setbuf(CRETE_FILE * stream, char * buf)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        setbuf_ty fn = (setbuf_ty)crete_dlsym_next("setbuf");
        fn(stream, buf);
        return;
    }

    assert(0 && "CRETE: unimplemented file routine called: setbuf");
    return;
}

typedef int (*setvbuf_ty)(void*, char*, int, size_t);
int setvbuf(CRETE_FILE * stream, char * buf, int type,
       size_t size)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        setvbuf_ty fn = (setvbuf_ty)crete_dlsym_next("setvbuf");
        return fn(stream, buf, type, size);
    }

    assert(0 && "CRETE: unimplemented file routine called: setvbuf");
    return 0;
}

// TODO: ensure "A successful intervening call (with the stream pointed to by stream) to a file-positioning function (fseek(), fseeko(), fsetpos(), or rewind()) or fflush() shall discard any pushed-back bytes for the stream."
typedef int (*ungetc_ty)(int, void*);
int ungetc(int c, CRETE_FILE *stream)
{
    stream = crete_get_associated_stream(stream);

    if(!crete_symfile_is_stream_concolic(stream))
    {
        ungetc_ty fn = (ungetc_ty)crete_dlsym_next("ungetc");
        return fn(c, stream);
    }

    if(c == CRETE_EOF)
        return CRETE_EOF;
    if(crete_symfile_has_pushed_back(stream))
        return CRETE_EOF;

    stream->pushed_back = (uint8_t)c;

    stream->eof = false;

    return c;
}

CRETE_FILE* crete_make_blank_concolic_stdin(void)
{
    assert(crete_stdin_concolic_handle_v.size == 0);

    crete_stdin_concolic_handle_v.size = 0;
    crete_stdin_concolic_handle_v.contents = NULL;
    crete_stdin_concolic_handle_v.concolic_file_index = UINT_MAX;
    crete_stdin_concolic_handle_v.concolic_file_handle_index = UINT_MAX;
    crete_stdin_concolic_handle_v.last_op = FILE_OP_NONE;
    crete_stdin_concolic_handle_v.mode = FILE_MODE_READ;
    crete_stdin_concolic_handle_v.is_open = true;
    crete_stdin_concolic_handle_v.eof = false;
    crete_stdin_concolic_handle_v.pushed_back = CRETE_EOF;

    return crete_stdin_concolic_handle;
}

#ifdef __cplusplus
}
#endif // __cplusplus






