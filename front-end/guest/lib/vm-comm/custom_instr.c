#include <crete/custom_instr.h>
#include <crete/custom_opcode.h>
#include <crete/debug_flags.h>

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

// for memory that needs to be touched.
static inline void __crete_touch_buffer(volatile void *buffer, unsigned size)
{
    unsigned i;
    volatile char *b = (volatile char *) buffer;
    for (i = 0; i < size; ++i) {
        *b; ++b;
    }
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
