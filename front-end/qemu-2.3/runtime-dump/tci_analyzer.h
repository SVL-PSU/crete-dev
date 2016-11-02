#ifndef CRETE_TCI_ANALYZER_H
#define CRETE_TCI_ANALYZER_H

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif // __STDC_LIMIT_MACROS

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
void crete_init_analyzer(uint64_t guest_vcpu_addr, uint64_t tcg_sp_value);

void crete_tci_next_block(void);
bool crete_tci_get_crete_read_was_symbolic(void);
void crete_tci_set_crete_read_was_symbolic(bool input);

bool crete_tci_is_current_block_symbolic(void);
bool crete_tci_is_previous_block_symbolic(void);
void crete_tci_mark_block_symbolic(void);
void crete_tci_next_iteration(void); // reset for taint analysis

void crete_tci_next_tci_instr(void); // Must call at the entry of each instruction.
void crete_tci_read_reg(uint64_t index, uint64_t data);
void crete_tci_write_reg(uint64_t index, uint64_t data);

void crete_tci_call_32(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4,
        uint64_t arg5, uint64_t arg6, uint64_t arg7, uint64_t arg8, uint64_t arg9);
void crete_tci_call_64(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4);

void crete_tci_ld8u_i32(uint64_t t0, uint64_t t1, uint64_t offset);
void crete_tci_ld_i32(uint64_t t0, uint64_t t1, uint64_t offset);
void crete_tci_st8_i32(uint64_t t1, uint64_t offset);
void crete_tci_st16_i32(uint64_t t1, uint64_t offset);
void crete_tci_st_i32(uint64_t t1, uint64_t offset);
void crete_tci_ld8u_i64(uint64_t t0, uint64_t t1, uint64_t offset);
void crete_tci_ld32u_i64(uint64_t t0, uint64_t t1, uint64_t offset);
void crete_tci_ld32s_i64(uint64_t t0, uint64_t t1, uint64_t offset);
void crete_tci_ld_i64(uint64_t t0, uint64_t t1, uint64_t offset);
void crete_tci_st8_i64(uint64_t t1, uint64_t offset);
void crete_tci_st16_i64(uint64_t t1, uint64_t offset);
void crete_tci_st32_i64(uint64_t t1, uint64_t offset);
void crete_tci_st_i64(uint64_t t1, uint64_t offset);

void crete_tci_qemu_ld8u(uint64_t t0, uint64_t addr, uint64_t data);
void crete_tci_qemu_ld8s(uint64_t t0, uint64_t addr, uint64_t data);
void crete_tci_qemu_ld16u(uint64_t t0, uint64_t addr, uint64_t data);
void crete_tci_qemu_ld16s(uint64_t t0, uint64_t addr, uint64_t data);
void crete_tci_qemu_ld32u(uint64_t t0, uint64_t addr, uint64_t data);
void crete_tci_qemu_ld32s(uint64_t t0, uint64_t addr, uint64_t data);
void crete_tci_qemu_ld32(uint64_t t0, uint64_t addr, uint64_t data);
void crete_tci_qemu_ld64(uint64_t t0, uint64_t addr, uint64_t data);
//void crete_tci_qemu_ld64_32(uint64_t t0, uint64_t addr, uint64_t data);
void crete_tci_qemu_st8(uint64_t addr, uint64_t data);
void crete_tci_qemu_st16(uint64_t addr, uint64_t data);
void crete_tci_qemu_st32(uint64_t addr, uint64_t data);
void crete_tci_qemu_st64(uint64_t addr, uint64_t data);

// Trace Graph
bool crete_tci_is_block_branching(void);
void crete_tci_brcond(void);

//Debug
void tci_analyzer_print(void);

#ifdef __cplusplus
}
#endif // __cplusplus

#ifdef __cplusplus

#include <vector>
// runtime-dump.cpp
void crete_tci_crete_make_concolic(uint64_t addr, uint64_t size,
        const std::vector<uint8_t>& data);

#endif // __cplusplus

#endif // CRETE_TCI_ANALYZER_H
