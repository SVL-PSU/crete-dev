/* Macros, globals and functions for debugging crete */

#ifndef CRETE_DEBUG_H
#define CRETE_DEBUG_H

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

//#define CRETE_CROSS_CHECK // Enable cross check

//#define CRETE_DBG_TA    // Debug taint-analysis
//#define CRETE_DBG_MEM   // Debug memory usage
//#define CRETE_DBG_MEM_MONI // Debug Memory monitoring

//#define CRETE_DEBUG_GENERAL // general debug info
//#define CRETE_DEBUG_TRACE_TAG // Debug trace tag
//#define CRETE_DEBUG_TRACE_KERNEL // Debug trace into kernel code
//#define CRETE_DEBUG_INTERRUPT// Debug interrupt

#ifdef CRETE_DEBUG_GENERAL
#define CRETE_DBG_GEN(x) do { x } while(0)
#else
#define CRETE_DBG_GEN(x) do { } while(0)
#endif

#ifdef CRETE_DEBUG_TRACE_TAG
#define CRETE_DBG_TT(x) do { x } while(0)
#else
#define CRETE_DBG_TT(x) do { } while(0)
#endif

#ifdef CRETE_DEBUG_TRACE_KERNEL
#define CRETE_DBG_TK(x) do { x } while(0)
#else
#define CRETE_DBG_TK(x) do { } while(0)
#endif

#ifdef CRETE_DEBUG_INTERRUPT
#define CRETE_DBG_INT(x) do { x } while(0)
#else
#define CRETE_DBG_INT(x) do { } while(0)
#endif

#define CRETE_DBG_REG cc_src
#define CRETE_GET_STRING(x) "cc_src"
#define CRETE_DBG_GEN_PRINT_TB_RANGE(x) ((x > 145) && (x < 150))

int is_in_list_crete_dbg_tb_pc(uint64_t tb_pc);
int is_in_list_crete_dbg_ta_guest_addr(uint64_t addr);

float crete_mem_usage(void);
void crete_print_x86_cpu_regs(uint64_t offset, uint64_t size, char *buf);
void print_x86_all_cpu_regs(void *qemuCpuState);
void crete_print_helper_function_name(uint64_t func_addr);

void dump_IR(void *, void *);
void dump_dbg_ir(const void *cpuState, const void *tb_ptr);
void print_guest_memory(void *env_cpuState, uint64_t addr, int len);

void crete_add_c_cpuState_offset(uint64_t offset, uint64_t size);

void crete_verify_cpuState_offset_c_cxx(void);

void crete_dbg_disable_stderr_stdout(void);
void crete_dbg_enable_stderr_stdout(void);

#ifdef __cplusplus
}
#endif

#endif //#ifndef CRETE_DEBUG_H
