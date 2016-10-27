/*
 * Modified from:
 *
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *
 * All contributors are listed in the S2E-AUTHORS file.
 */

#ifndef CRETE_CUSTOM_INSTR_H
#define CRETE_CUSTOM_INSTR_H

#include <crete/guest-defs.h>

#if 1 || defined(DBG_BO_S2E_PRINT)
//#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdarg.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void crete_send_elf_data_info(uintptr_t addr, uintptr_t size);
void crete_send_elf_data(uintptr_t data, uintptr_t size);
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

/** Forces the read of every byte of the specified string.
  * This makes sure the memory pages occupied by the string are paged in
  * before passing them to S2E, which can't page in memory by itself. */
void __s2e_touch_string(volatile const char *string);
void __s2e_touch_buffer(volatile void *buffer, unsigned size);
/** Print message to the S2E log. */
void s2e_message(const char *message);
int s2e_printf(const char *format, ...);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // CRETE_CUSTOM_INSTR_H
