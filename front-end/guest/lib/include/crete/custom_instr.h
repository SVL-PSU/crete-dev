#ifndef CRETE_CUSTOM_INSTR_H
#define CRETE_CUSTOM_INSTR_H

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// For crete-run/preload-run
void crete_insert_instr_read_port(uintptr_t addr, size_t size);
void crete_send_custom_instr_prime(void);
void crete_send_target_pid(void);
void crete_void_target_pid(void);
void crete_send_custom_instr_dump(void);

// For program under test
void crete_make_concolic(void* addr, size_t size, const char* name);
#define crete_assume(cond)                      \
  crete_assume_begin();                         \
  crete_assume_(cond)

// unused, being kept for future usage
void crete_insert_instr_addr_exclude_filter(uintptr_t addr_begin, uintptr_t addr_end);
void crete_insert_instr_addr_include_filter(uintptr_t addr_begin, uintptr_t addr_end);
void crete_send_custom_instr_quit(void);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // CRETE_CUSTOM_INSTR_H
