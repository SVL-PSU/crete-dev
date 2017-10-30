#include <crete/custom_instr.h>
#include <crete/custom_opcode.h>
#include <crete/debug_flags.h>

// for memory that needs to be touched.
static inline void __crete_touch_buffer(volatile void *buffer, unsigned size)
{
    unsigned i;
    volatile char *b = (volatile char *) buffer;
    for (i = 0; i < size; ++i) {
        *b; ++b;
    }
}

//--------------------- For crete-run/preload-run
void crete_insert_instr_read_port(uintptr_t addr, uintptr_t size)
{
    __crete_touch_buffer((void *)addr, size);

    __asm__ __volatile__(
        CRETE_INSTR_READ_PORT()
        : : "a" (addr), "c" (size)
    );
}

void crete_send_custom_instr_prime()
{
    __asm__ __volatile__(
            CRETE_INSTR_PRIME()
    );
}

void crete_send_target_pid(void)
{
    __asm__ __volatile__(
            CRETE_INSTR_SEND_TARGET_PID()
    );
}

void crete_void_target_pid(void)
{
    __asm__ __volatile__(
            CRETE_INSTR_VOID_TARGET_PID()
    );
}

void crete_send_custom_instr_dump()
{
    __asm__ __volatile__(
        CRETE_INSTR_DUMP()
    );
}

//------ For program under test
// Function to be captured for replay "crete_make_concolic()":
// 1. Capture values of concolic variable and its name by touching/reading buffer
// 2. Inject a call to helper_crete_make_concolic() so that SVM can inject symbolic values
static void __crete_make_concolic_internal(void *addr, size_t size, const char* name)
{
    // Touch the content of name and force it is being captured
    {
        size_t name_size = strlen(name);
        volatile char *_addr = (volatile char *) addr;
        volatile char *_name = (volatile char *) name;

        size_t i;
        for (i = 0; (i < size && i < name_size); ++i) {
            *_addr; *_name;
            ++_name; ++_addr;
        }

        if(i < name_size) {
            --_addr;
            for(; i < name_size; ++i)
            {
                *_addr; *_name;
                ++_name;
            }
        } else if (i < size) {
            for(; i < size; ++i)
            {
                *_addr; ++_addr;
            }
        }
    }

    __asm__ __volatile__(
        CRETE_INSTR_MAKE_CONCOLIC_INTERNAL()
        : : "a" (addr), "c" (size), "d" (name)
    );
}

static void crete_send_concolic_name(const char* name) {
    size_t name_size = strlen(name);
    __crete_touch_buffer((void*)name, name_size);

    __asm__ __volatile__(
            CRETE_INSTR_SEND_CONCOLIC_NAME()
        : : "a" (name), "c" (name_size)
    );
}

// Prepare for "crete_make_concolic()" within VM for tracing:
// Send information from guest to VM, guest_addr, size, name and name size of concolic value, so that:
// 1. Set the correct value of concolic variale from test case
// 2. Enable taint-analysis on this concolic variable
static void crete_pre_make_concolic(void* addr, size_t size, const char* name)
{
    // CRETE_INSTR_SEND_CONCOLIC_NAME must be sent before CRETE_INSTR_PRE_MAKE_CONCOLIC,
    // to satisfy the hanlder's order in qemu/runtime-dump/custom-instruction.cpp
    crete_send_concolic_name(name);

    __crete_touch_buffer(addr, size);

    __asm__ __volatile__(
            CRETE_INSTR_PRE_MAKE_CONCOLIC()
        : : "a" (addr), "c" (size)
    );
}

void crete_make_concolic(void* addr, size_t size, const char* name)
{
    crete_pre_make_concolic(addr, size, name);
    __crete_make_concolic_internal(addr, size, name);
}

void crete_assume_begin()
{
    __asm__ __volatile__(
            CRETE_INSTR_ASSUME_BEGIN()
    );
}

void crete_assume_(int cond)
{
    __asm__ __volatile__(
        CRETE_INSTR_ASSUME()
        : : "a" (cond)
    );
}

//------ unused, being kept for future usage
void crete_insert_instr_addr_exclude_filter(uintptr_t addr_begin, uintptr_t addr_end)
{
    __asm__ __volatile__(
        CRETE_INSTR_EXCLUDE_FILTER()
        : : "a" (addr_begin), "c" (addr_end)
    );
}

void crete_insert_instr_addr_include_filter(uintptr_t addr_begin, uintptr_t addr_end)
{
    __asm__ __volatile__(
        CRETE_INSTR_INCLUDE_FILTER()
        : : "a" (addr_begin), "c" (addr_end)
    );
}

void crete_send_custom_instr_quit()
{
    __asm__ __volatile__(
        CRETE_INSTR_QUIT()
    );
}

