/*
 *  x86 exception helpers
 *
 *  Copyright (c) 2003 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "cpu.h"
#include "qemu/log.h"
#include "sysemu/sysemu.h"
#include "exec/helper-proto.h"

#if defined(CRETE_CONFIG) || 1
#include "runtime-dump/crete-debug.h"
#include "runtime-dump/runtime-dump.h"
extern CPUArchState *g_cpuState_bct;
#endif // #if defined(CRETE_CONFIG)
//#define DEBUG_PCALL

#if 0
#define raise_exception_err(env, a, b)                                  \
    do {                                                                \
        qemu_log("raise_exception line=%d\n", __LINE__);                \
        (raise_exception_err)(env, a, b);                               \
    } while (0)
#endif

void helper_raise_interrupt(CPUX86State *env, int intno, int next_eip_addend)
{
    raise_interrupt(env, intno, 1, 0, next_eip_addend);
}

void helper_raise_exception(CPUX86State *env, int exception_index)
{
    raise_exception(env, exception_index);
}

/*
 * Check nested exceptions and change to double or triple fault if
 * needed. It should only be called, if this is not an interrupt.
 * Returns the new exception number.
 */
static int check_exception(CPUX86State *env, int intno, int *error_code)
{
    int first_contributory = env->old_exception == 0 ||
                              (env->old_exception >= 10 &&
                               env->old_exception <= 13);
    int second_contributory = intno == 0 ||
                               (intno >= 10 && intno <= 13);

    qemu_log_mask(CPU_LOG_INT, "check_exception old: 0x%x new 0x%x\n",
                env->old_exception, intno);

#if !defined(CONFIG_USER_ONLY)
    if (env->old_exception == EXCP08_DBLE) {
        if (env->hflags & HF_SVMI_MASK) {
            cpu_vmexit(env, SVM_EXIT_SHUTDOWN, 0); /* does not return */
        }

        qemu_log_mask(CPU_LOG_RESET, "Triple fault\n");

        qemu_system_reset_request();
        return EXCP_HLT;
    }
#endif

    if ((first_contributory && second_contributory)
        || (env->old_exception == EXCP0E_PAGE &&
            (second_contributory || (intno == EXCP0E_PAGE)))) {
        intno = EXCP08_DBLE;
        *error_code = 0;
    }

    if (second_contributory || (intno == EXCP0E_PAGE) ||
        (intno == EXCP08_DBLE)) {
        env->old_exception = intno;
    }

    return intno;
}

/*
 * Signal an interruption. It is executed in the main CPU loop.
 * is_int is TRUE if coming from the int instruction. next_eip is the
 * env->eip value AFTER the interrupt instruction. It is only relevant if
 * is_int is TRUE.
 */
static void QEMU_NORETURN raise_interrupt2(CPUX86State *env, int intno,
                                           int is_int, int error_code,
                                           int next_eip_addend)
{
#if defined(CRETE_CONFIG) || 1
    // Ignore all the interrupts caused by loading code:
    // Neither log it, nor call crete_post_cpu_tb_exec()
    if(!f_crete_is_loading_code)
    {
        CRETE_DBG_INT(
        fprintf(stderr, "[Interrupt] f_crete_enabled = %d: "
                "tb-%lu (pc-%p) calls into raise_interrupt2().\n",
                f_crete_enabled, rt_dump_tb_count - 1, (void *)(uint64_t)rt_dump_tb->pc);

        fprintf(stderr, "Interrupt Info: intno = %d, is_int = %d, "
                "error_code = %d, next_eip_addend = %d,"
                "env->eip = %p\n",
                intno, is_int, error_code,
                next_eip_addend, (void *)(uint64_t)env->eip);

        if(is_int && next_eip_addend)
        {
            fprintf(stderr, "[CRETE Warning] next_eip_addend is not zero. Check whether the precise "
                    "interrupt reply is correct. [check gen_intermediate_code_crete()]\n");
        }
        );

        // All the other interrupts from the middle of a TB execution
        // should go into the same closure (crete_post_cpu_tb_exec())
        // 0 means the current TB has been executed but stopped in the middle (until env->eip)
        if(crete_post_cpu_tb_exec(env, rt_dump_tb, 0, env->eip))
        {
            // FIXME: xxx should not be necessary, as interrupt should be totally invisible while replay
            add_qemu_interrupt_state(runtime_env, intno, is_int, error_code, next_eip_addend);
        }

        // When the interrupt is raised while g_crete_enabled is on,
        // call set_interrupt_process_info() to set the return-pc of the interrupt.
        // g_crete_enabled is being turned off until the interrupt returns to the return-pc
        if(f_crete_enabled)
        {
            // FIXME: xxx check whether next_eip_addend should always be added to the current eip for
            //      calculating the return addr of the interrupt
            set_interrupt_process_info(runtime_env, (target_ulong)(env->eip + next_eip_addend));
        }
    } else {
        f_crete_is_loading_code = 0;
    }
#endif

    CPUState *cs = CPU(x86_env_get_cpu(env));

    if (!is_int) {
        cpu_svm_check_intercept_param(env, SVM_EXIT_EXCP_BASE + intno,
                                      error_code);
        intno = check_exception(env, intno, &error_code);
    } else {
        cpu_svm_check_intercept_param(env, SVM_EXIT_SWINT, 0);
    }

    cs->exception_index = intno;
    env->error_code = error_code;
    env->exception_is_int = is_int;
    env->exception_next_eip = env->eip + next_eip_addend;
    cpu_loop_exit(cs);
}

/* shortcuts to generate exceptions */

void QEMU_NORETURN raise_interrupt(CPUX86State *env, int intno, int is_int,
                                   int error_code, int next_eip_addend)
{
    raise_interrupt2(env, intno, is_int, error_code, next_eip_addend);
}

void raise_exception_err(CPUX86State *env, int exception_index,
                         int error_code)
{
    raise_interrupt2(env, exception_index, 0, error_code, 0);
}

void raise_exception(CPUX86State *env, int exception_index)
{
    raise_interrupt2(env, exception_index, 0, 0, 0);
}
