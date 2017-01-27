#ifndef CRETE_CUSTOM_INSTR_H
#define CRETE_CUSTOM_INSTR_H

#include <crete/guest-defs.h>

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void crete_capture_begin(void);
void crete_capture_end(void);
void crete_insert_instr_addr_exclude_filter(uintptr_t addr_begin, uintptr_t addr_end);
void crete_insert_instr_addr_include_filter(uintptr_t addr_begin, uintptr_t addr_end);
void crete_insert_instr_call_stack_exclude(uintptr_t addr_begin, uintptr_t addr_end);
void crete_insert_instr_call_stack_size(uintptr_t size);
void crete_insert_instr_stack_depth_bounds(intptr_t low, intptr_t high);
void crete_insert_instr_main_address(uintptr_t addr, uintptr_t size);
void crete_insert_instr_libc_exit_address(uintptr_t addr, uintptr_t size);
void crete_insert_instr_libc_start_main_address(uintptr_t addr, uintptr_t size);
void crete_send_function_entry(uintptr_t addr, uintptr_t size, const char* name);
void crete_send_custom_instr_quit();
void crete_send_custom_instr_dump();
void __crete_make_symbolic(); // No inlining. Needs to be separate function to list in dump list.
void crete_send_custom_instr_prime();
void crete_send_custom_instr_start_stopwatch();
void crete_send_custom_instr_stop_stopwatch();
void crete_send_custom_instr_reset_stopwatch();
void crete_insert_instr_next_replay_program(uintptr_t addr, uintptr_t size);
void crete_insert_instr_read_port(uintptr_t addr, uintptr_t size);

void crete_make_concolic(void* addr, size_t size, const char* name);

#define crete_assume(cond)                      \
  crete_assume_begin();                         \
  crete_assume_(cond)

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // CRETE_CUSTOM_INSTR_H
