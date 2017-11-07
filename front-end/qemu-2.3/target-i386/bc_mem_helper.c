#include "cpu.h"

void helper_lock(void)
{
  ;
}

void helper_unlock(void)
{
  ;
}

void helper_cmpxchg8b(CPUX86State *env, target_ulong a0)
{
    uint64_t d;
    int eflags;

    eflags = cpu_cc_compute_all(env, CC_OP);
    d = cpu_ldq_data(env, a0);
    if (d == (((uint64_t)env->regs[R_EDX] << 32) | (uint32_t)env->regs[R_EAX])) {
        cpu_stq_data(env, a0, ((uint64_t)env->regs[R_ECX] << 32) | (uint32_t)env->regs[R_EBX]);
        eflags |= CC_Z;
    } else {
        /* always do the store */
        cpu_stq_data(env, a0, d);
        env->regs[R_EDX] = (uint32_t)(d >> 32);
        env->regs[R_EAX] = (uint32_t)d;
        eflags &= ~CC_Z;
    }
    CC_SRC = eflags;
}
