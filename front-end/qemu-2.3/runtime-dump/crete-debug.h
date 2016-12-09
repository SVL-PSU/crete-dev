/* Macros, globals and functions for debugging crete */

#ifndef CRETE_DEBUG_H
#define CRETE_DEBUG_H

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

//#define CRETE_CROSS_CHECK // Enable cross check
//#define CRETE_DBG_CK    // Debug cross-check
//#define CRETE_DBG_TA    // Debug taint-analysis
//#define CRETE_DBG_MEM   // Debug memory usage
//#define CRETE_DBG_MEM_MONI // Debug Memory monitoring
#define CRETE_DBG_TODO    // Debug TODO work

#define CRETE_DBG_REG fpregs[7]
#define CRETE_GET_STRING(x) "fpregs[7]"

int is_in_list_crete_dbg_tb_pc(uint64_t tb_pc);
int is_in_list_crete_dbg_ta_guest_addr(uint64_t addr);

float crete_mem_usage(void);
void crete_print_x86_cpu_regs(uint64_t offset, uint64_t size, char *buf);
void print_x86_all_cpu_regs(void *qemuCpuState);
void crete_print_helper_function_name(uint64_t func_addr);

void dump_IR(void *, unsigned long long);
void print_guest_memory(void *env_cpuState, uint64_t addr, int len);

void crete_add_c_cpuState_offset(uint64_t offset, uint64_t size);

void crete_verify_cpuState_offset_c_cxx(void);

#ifdef __cplusplus
}
#endif

#endif //#ifndef CRETE_DEBUG_H
