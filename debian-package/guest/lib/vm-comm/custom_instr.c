#include <crete/custom_instr.h>
#include <crete/custom_opcode.h>
#include <crete/debug_flags.h>

#define S2E_INSTRUCTION_COMPLEX(val1, val2)             \
    ".byte 0x0F, 0x3F\n"                                \
    ".byte 0x00, 0x" #val1 ", 0x" #val2 ", 0x00\n"      \
    ".byte 0x00, 0x00, 0x00, 0x00\n"

#define S2E_INSTRUCTION_SIMPLE(val)                     \
    S2E_INSTRUCTION_COMPLEX(val, 00)

void crete_send_elf_data_info(uintptr_t addr, uintptr_t size)
{
    __asm__ __volatile__(
            S2E_INSTRUCTION_COMPLEX(AE, 01)
        : : "c" (addr), "a" (size)
    );
}

void crete_send_elf_data(uintptr_t data, uintptr_t size)
{
    __asm__ __volatile__(
            S2E_INSTRUCTION_COMPLEX(AE, 02)
        : : "c" (data), "a" (size)
    );
}

void crete_capture_begin(void)
{
    __asm__ __volatile__(
            CRETE_INSTR_CAPTURE_BEGIN()
    );
}

void crete_capture_end(void)
{
    __asm__ __volatile__(
            CRETE_INSTR_CAPTURE_END()
    );
}

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

void crete_send_custom_instr_dump()
{
    __asm__ __volatile__(
        CRETE_INSTR_DUMP()
    );
}

/** Forces the read of every byte of the specified string.
  * This makes sure the memory pages occupied by the string are paged in
  * before passing them to S2E, which can't page in memory by itself. */

void __s2e_touch_string(volatile const char *string)
{
    while (*string) {
        ++string;
    }
}

// crete_make_concolic requires this not to be captured (messes up the MO name in Klee).
void __s2e_touch_buffer(volatile void *buffer, unsigned size)
{
    unsigned i;
    volatile char *b = (volatile char *) buffer;
    for (i = 0; i < size; ++i) {
        *b; ++b;
    }
}

// Use this as an alternative to __s2e_touch_buffer for memory that needs to be touched.
static inline void __crete_touch_buffer(volatile void *buffer, unsigned size)
{
    unsigned i;
    volatile char *b = (volatile char *) buffer;
    for (i = 0; i < size; ++i) {
        *b; ++b;
    }
}

/** Print message to the S2E log. */
void s2e_message(const char *message)
{
    __s2e_touch_string(message);
    __asm__ __volatile__(
        CRETE_INSTR_MESSAGE()
        : : "a" (message)
    );
}

int s2e_printf(const char *format, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    s2e_message(buffer);
    return ret;
}

// Inject call to helper_crete_make_symbolic, so that klee can catch
void __crete_make_concolic_internal(void)
{
    __asm__ __volatile__(
        CRETE_INSTR_MAKE_SYMBOLIC()
    );
}

void crete_send_concolic_name(const char* name) {
    size_t name_size = strlen(name);
    __crete_touch_buffer((void*)name, name_size);

    __asm__ __volatile__(
            CRETE_INSTR_SEND_CONCOLIC_NAME()
        : : "a" (name), "c" (name_size)
    );
}

void crete_make_concolic(void* addr, size_t size, const char* name)
{
    // CRETE_INSTR_SEND_CONCOLIC_NAME must be sent before CRETE_INSTR_MAKE_CONCOLIC,
    // to satisfy the hanlder's order in qemu/runtime-dump/custom-instruction.cpp
    crete_send_concolic_name(name);

    __crete_touch_buffer(addr, size);

    __asm__ __volatile__(
            CRETE_INSTR_MAKE_CONCOLIC()
        : : "a" (addr), "c" (size)
    );

    __crete_make_concolic_internal();
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


void crete_send_custom_instr_prime()
{
    __asm__ __volatile__(
            CRETE_INSTR_PRIME()
    );
}

void crete_send_function_entry(uintptr_t addr, uintptr_t size, const char* name)
{
    __s2e_touch_buffer((void*)name, strlen(name));

    __asm__ __volatile__(
        S2E_INSTRUCTION_COMPLEX(AE, 07)
        : : "a" (addr), "c" (size), "d" (name)
    );
}

// --- Debugging Routines ---

void crete_make_concrete(void* addr, size_t size, const char* name)
{
    __crete_touch_buffer(addr, size);

    __asm__ __volatile__(
        CRETE_INSTR_MAKE_CONCRETE()
        : : "a" (addr), "c" (size), "d" (name)
    );
}

void crete_debug_print_f32(float val)
{
    __asm__ __volatile__(
        CRETE_INSTR_DEBUG_PRINT_F32()
        : : "a" (val)
    );
}

void crete_debug_print_buf(const void* addr, size_t size, const char* name)
{
    __crete_touch_buffer((void*)addr, size);

    crete_make_concrete((void*)name, strlen(name) + 1, "");

    __asm__ __volatile__(
        CRETE_INSTR_DEBUG_PRINT_BUF()
        : : "a" (addr), "c" (size), "d" (name)
    );
}

void crete_debug_assert_is_concolic(const void* addr, size_t size, const char* name)
{
    __crete_touch_buffer((void*)addr, size);

    crete_make_concrete((void*)name, strlen(name) + 1, "");

    __asm__ __volatile__(
        CRETE_INSTR_DEBUG_ASSERT_IS_CONCOLIC()
        : : "a" (addr), "c" (size), "d" (name)
    );
}

void crete_debug_monitor_concolic_status(const void* addr, size_t size, const char* name)
{
    __crete_touch_buffer((void*)addr, size);

    crete_make_concrete((void*)name, strlen(name) + 1, "");

    __asm__ __volatile__(
        CRETE_INSTR_DEBUG_MONITOR_CONCOLIC_STATUS()
        : : "a" (addr), "c" (size), "d" (name)
    );
}

void crete_debug_monitor_value(const void* addr, size_t size, const char* name)
{
    __crete_touch_buffer((void*)addr, size);

    crete_make_concrete((void*)name, strlen(name) + 1, "");

    __asm__ __volatile__(
        CRETE_INSTR_DEBUG_MONITOR_VALUE()
        : : "a" (addr), "c" (size), "d" (name)
    );
}

void crete_debug_monitor_set_flag(enum CreteDebugFlag flag, const char* name)
{
    crete_make_concrete((void*)name, strlen(name) + 1, "");

    __asm__ __volatile__(
        CRETE_INSTR_DEBUG_MONITOR_SET_FLAG()
        : : "a" ((uintptr_t)flag), "c" (name)
    );
}

void crete_debug_capture(enum CreteDebugFlag flag)
{
    __asm__ __volatile__(
        CRETE_INSTR_DEBUG_CAPTURE()
        : : "a" ((uintptr_t)flag)
    );
}


void crete_insert_instr_call_stack_exclude(uintptr_t addr_begin, uintptr_t addr_end)
{
    __asm__ __volatile__(
        CRETE_INSTR_CALL_STACK_EXCLUDE()
        : : "a" (addr_begin), "c" (addr_end)
    );
}


void crete_insert_instr_call_stack_size(uintptr_t size)
{
    __asm__ __volatile__(
        CRETE_INSTR_CALL_STACK_SIZE()
        : : "a" (size)
    );
}


void crete_insert_instr_main_address(uintptr_t addr, uintptr_t size)
{
    __asm__ __volatile__(
        CRETE_INSTR_MAIN_ADDRESS()
        : : "a" (addr), "c" (size)
    );
}

void crete_insert_instr_libc_start_main_address(uintptr_t addr, uintptr_t size)
{
    __asm__ __volatile__(
        CRETE_INSTR_LIBC_START_MAIN_ADDRESS()
        : : "a" (addr), "c" (size)
    );
}

void crete_insert_instr_stack_depth_bounds(intptr_t low, intptr_t high)
{
    __asm__ __volatile__(
        CRETE_INSTR_STACK_DEPTH_BOUNDS()
        : : "a" (low), "c" (high)
    );
}

void crete_insert_instr_libc_exit_address(uintptr_t addr, uintptr_t size)
{
    __asm__ __volatile__(
        CRETE_INSTR_LIBC_EXIT_ADDRESS()
        : : "a" (addr), "c" (size)
    );
}

void crete_send_custom_instr_start_stopwatch(void)
{
    __asm__ __volatile__(
        CRETE_INSTR_START_STOPWATCH()
    );
}

void crete_send_custom_instr_stop_stopwatch(void)
{
    __asm__ __volatile__(
        CRETE_INSTR_STOP_STOPWATCH()
    );
}

void crete_send_custom_instr_reset_stopwatch(void)
{
    __asm__ __volatile__(
        CRETE_INSTR_RESET_STOPWATCH()
    );
}

void crete_insert_instr_next_replay_program(uintptr_t addr, uintptr_t size)
{
    __asm__ __volatile__(
        CRETE_INSTR_REPLAY_NEXT_PROGRAM()
        : : "a" (addr), "c" (size)
    );
}

void crete_insert_instr_read_port(uintptr_t addr, uintptr_t size)
{
    __crete_touch_buffer((void *)addr, size);

    __asm__ __volatile__(
        CRETE_INSTR_READ_PORT()
        : : "a" (addr), "c" (size)
    );
}
