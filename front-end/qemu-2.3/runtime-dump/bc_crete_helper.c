#include "config.h"
#include "cpu.h"
#include "tcg.h"

#include "stdint.h"
#include "crete-debug.h"

#if defined(CRETE_CROSS_CHECK)
void crete_verify_cpuState_offset(const char *name, uint64_t offset, uint64_t size);

#define __VERIFY_CPUSTATE_OFFSET(in_type, in_name)      \
        {                                               \
    char name[] = #in_name;                             \
    offset = offsetof(CPUArchState, in_name);           \
    size = sizeof(in_type);                             \
    crete_verify_cpuState_offset(name, offset, size);   \
        }

#define __VERIFY_CPUSTATE_OFFSET_ARRAY(in_type, in_name, array_size)    \
        {                                                               \
    char index = '0';                                                   \
    for(i = 0; i < (array_size); ++i)                                   \
    {                                                                   \
        char name[100] = #in_name;                                      \
        uint64_t j =0;                                                  \
        while(name[j]!= '\0')                                           \
           ++j;                                                         \
        name[j++] = '[';                                                \
        name[j++] = index++;                                            \
        name[j++] = ']';                                                \
        name[j] = '\0';                                                 \
        offset = offsetof(CPUArchState, in_name) + i*sizeof(in_type);   \
        size = sizeof(in_type);                                         \
        crete_verify_cpuState_offset(name, offset, size);               \
    }                                                                   \
        }

void crete_verify_all_cpuState_offset()
{
    uint64_t i = 0;
    uint64_t offset = 0;
    uint64_t size = 0;

    __VERIFY_CPUSTATE_OFFSET(target_ulong, regs);

    __VERIFY_CPUSTATE_OFFSET(target_ulong, eflags)

    /* emulator internal eflags handling */
    // target_ulong cc_dst;
    __VERIFY_CPUSTATE_OFFSET(target_ulong, cc_dst)
    // target_ulong cc_src;
    __VERIFY_CPUSTATE_OFFSET(target_ulong, cc_src)
    // target_ulong cc_src2;
    __VERIFY_CPUSTATE_OFFSET(target_ulong, cc_src2)
    //uint32_t cc_op;
    __VERIFY_CPUSTATE_OFFSET(uint32_t, cc_op);
    // int32_t df;
    __VERIFY_CPUSTATE_OFFSET(int32_t, df)
    // uint32_t hflags;
    __VERIFY_CPUSTATE_OFFSET(uint32_t, hflags)
    // uint32_t hflags2;
    __VERIFY_CPUSTATE_OFFSET(uint32_t, hflags2)

    /* segments */
    // SegmentCache segs[6];
    __VERIFY_CPUSTATE_OFFSET_ARRAY(SegmentCache, segs, 6)

    // SegmentCache ldt;
    __VERIFY_CPUSTATE_OFFSET(SegmentCache, ldt)
    // SegmentCache tr;
    __VERIFY_CPUSTATE_OFFSET(SegmentCache, tr)
    // SegmentCache gdt;
    __VERIFY_CPUSTATE_OFFSET(SegmentCache, gdt)
    // SegmentCache idt;
    __VERIFY_CPUSTATE_OFFSET(SegmentCache, idt)

    // xxx: not traced
    // target_ulong cr[5];
    //    __VERIFY_CPUSTATE_OFFSET_ARRAY(target_ulong, cr, 5)
    // int32_t a20_mask;
    __VERIFY_CPUSTATE_OFFSET(int32_t, a20_mask)

    // BNDReg bnd_regs[4];
    __VERIFY_CPUSTATE_OFFSET_ARRAY(BNDReg, bnd_regs, 4)
    // BNDCSReg bndcs_regs;
    __VERIFY_CPUSTATE_OFFSET(BNDCSReg, bndcs_regs)
    // uint64_t msr_bndcfgs;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, msr_bndcfgs)

    /* Beginning of state preserved by INIT (dummy marker).  */
    // xxx: not traced
    //    struct {} start_init_save;
    //    __VERIFY_CPUSTATE_OFFSET(struct {}, start_init_save)

    /* FPU state */
    // unsigned int fpstt;
    __VERIFY_CPUSTATE_OFFSET(unsigned int, fpstt)
    // uint16_t fpus;
    __VERIFY_CPUSTATE_OFFSET(uint16_t, fpus)
    // uint16_t fpuc;
    __VERIFY_CPUSTATE_OFFSET(uint16_t, fpuc)
    // uint8_t fptags[8];
    __VERIFY_CPUSTATE_OFFSET_ARRAY(uint8_t, fptags, 8)
    // FPReg fpregs[8];
    __VERIFY_CPUSTATE_OFFSET_ARRAY(FPReg, fpregs, 8)
    /* KVM-only so far */
    // uint16_t fpop;
    __VERIFY_CPUSTATE_OFFSET(uint16_t, fpop)
    // uint64_t fpip;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, fpip)
    // uint64_t fpdp;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, fpdp)

    /* emulator internal variables */
    // float_status fp_status;
    __VERIFY_CPUSTATE_OFFSET(float_status, fp_status)
    // floatx80 ft0;
    __VERIFY_CPUSTATE_OFFSET(floatx80, ft0)

    // float_status mmx_status;
    __VERIFY_CPUSTATE_OFFSET(float_status, mmx_status)
    // float_status sse_status;
    __VERIFY_CPUSTATE_OFFSET(float_status, sse_status)
    // uint32_t mxcsr;
    __VERIFY_CPUSTATE_OFFSET(uint32_t, mxcsr)
    // XMMReg xmm_regs[CPU_NB_REGS == 8 ? 8 : 32];
    __VERIFY_CPUSTATE_OFFSET_ARRAY(XMMReg, xmm_regs, CPU_NB_REGS == 8 ? 8 : 32)
    // XMMReg xmm_t0;
    __VERIFY_CPUSTATE_OFFSET(XMMReg, xmm_t0)
    // MMXReg mmx_t0;
    __VERIFY_CPUSTATE_OFFSET(MMXReg, mmx_t0)

    // uint64_t opmask_regs[NB_OPMASK_REGS];
    __VERIFY_CPUSTATE_OFFSET_ARRAY(uint64_t, opmask_regs, NB_OPMASK_REGS)

    /* sysenter registers */
    // uint32_t sysenter_cs;
    __VERIFY_CPUSTATE_OFFSET(uint32_t, sysenter_cs)
    // target_ulong sysenter_esp;
    __VERIFY_CPUSTATE_OFFSET(target_ulong, sysenter_esp)
    // target_ulong sysenter_eip;
    __VERIFY_CPUSTATE_OFFSET(target_ulong, sysenter_eip)
    // uint64_t efer;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, efer)
    // uint64_t star;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, star)

    // uint64_t vm_hsave;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, vm_hsave)

#ifdef TARGET_X86_64
    // target_ulong lstar;
    __VERIFY_CPUSTATE_OFFSET(target_ulong, lstar)
    // target_ulong cstar;
    __VERIFY_CPUSTATE_OFFSET(target_ulong, cstar)
    // target_ulong fmask;
    __VERIFY_CPUSTATE_OFFSET(target_ulong, fmask)
    // target_ulong kernelgsbase;
    __VERIFY_CPUSTATE_OFFSET(target_ulong, kernelgsbase)
#endif

    // uint64_t tsc;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, tsc)
    // uint64_t tsc_adjust;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, tsc_adjust)
    // uint64_t tsc_deadline;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, tsc_deadline)

    // uint64_t mcg_status;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, mcg_status)
    // uint64_t msr_ia32_misc_enable;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, msr_ia32_misc_enable)
    // uint64_t msr_ia32_feature_control;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, msr_ia32_feature_control)

    // uint64_t msr_fixed_ctr_ctrl;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, msr_fixed_ctr_ctrl)
    // uint64_t msr_global_ctrl;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, msr_global_ctrl)
    // uint64_t msr_global_status;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, msr_global_status)
    // uint64_t msr_global_ovf_ctrl;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, msr_global_ovf_ctrl)
    // uint64_t msr_fixed_counters[MAX_FIXED_COUNTERS];
    __VERIFY_CPUSTATE_OFFSET_ARRAY(uint64_t, msr_fixed_counters, MAX_FIXED_COUNTERS)
    // uint64_t msr_gp_counters[MAX_GP_COUNTERS];
    __VERIFY_CPUSTATE_OFFSET_ARRAY(uint64_t, msr_gp_counters, MAX_GP_COUNTERS)
    // uint64_t msr_gp_evtsel[MAX_GP_COUNTERS];
    __VERIFY_CPUSTATE_OFFSET_ARRAY(uint64_t, msr_gp_evtsel, MAX_GP_COUNTERS)

    // uint64_t pat;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, pat)
    // uint32_t smbase;
    __VERIFY_CPUSTATE_OFFSET(uint32_t, smbase)

    /* End of state preserved by INIT (dummy marker).  */
    // xxx: not traced
    //    struct {} end_init_save;
    //    __VERIFY_CPUSTATE_OFFSET(struct {}, end_init_save)

    // uint64_t system_time_msr;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, system_time_msr)
    // uint64_t wall_clock_msr;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, wall_clock_msr)
    // uint64_t steal_time_msr;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, steal_time_msr)
    // uint64_t async_pf_en_msr;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, async_pf_en_msr)
    // uint64_t pv_eoi_en_msr;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, pv_eoi_en_msr)

    // uint64_t msr_hv_hypercall;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, msr_hv_hypercall)
    // uint64_t msr_hv_guest_os_id;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, msr_hv_guest_os_id)
    // uint64_t msr_hv_vapic;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, msr_hv_vapic)
    // uint64_t msr_hv_tsc;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, msr_hv_tsc)

    /* exception/interrupt handling */
    //xxx: not traced
    // int error_code;
    //    __VERIFY_CPUSTATE_OFFSET(int, error_code)
    // int exception_is_int;
    __VERIFY_CPUSTATE_OFFSET(int, exception_is_int)
    // target_ulong exception_next_eip;
    __VERIFY_CPUSTATE_OFFSET(target_ulong, exception_next_eip)
    // target_ulong dr[8];
    __VERIFY_CPUSTATE_OFFSET_ARRAY(target_ulong, dr, 8)
    //xxx: not traced
    //    union {
    //        struct CPUBreakpoint *cpu_breakpoint[4];
    //        struct CPUWatchpoint *cpu_watchpoint[4];
    //    };
    // int old_exception;
    __VERIFY_CPUSTATE_OFFSET(int, old_exception)
    // uint64_t vm_vmcb;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, vm_vmcb)
    // uint64_t tsc_offset;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, tsc_offset)
    // uint64_t intercept;
    __VERIFY_CPUSTATE_OFFSET(uint64_t, intercept)
    // uint16_t intercept_cr_read;
    __VERIFY_CPUSTATE_OFFSET(uint16_t, intercept_cr_read)
    // uint16_t intercept_cr_write;
    __VERIFY_CPUSTATE_OFFSET(uint16_t, intercept_cr_write)
    // uint16_t intercept_dr_read;
    __VERIFY_CPUSTATE_OFFSET(uint16_t, intercept_dr_read)
    // uint16_t intercept_dr_write;
    __VERIFY_CPUSTATE_OFFSET(uint16_t, intercept_dr_write)
    // uint32_t intercept_exceptions;
    __VERIFY_CPUSTATE_OFFSET(uint32_t, intercept_exceptions)
    // uint8_t v_tpr;
    __VERIFY_CPUSTATE_OFFSET(uint8_t, v_tpr)

    /* KVM states, automatically cleared on reset */
    // uint8_t nmi_injected;
    __VERIFY_CPUSTATE_OFFSET(uint8_t, nmi_injected)
    // uint8_t nmi_pending;
    __VERIFY_CPUSTATE_OFFSET(uint8_t, nmi_pending)
}
#endif //#if defined(CRETE_CROSS_CHECK)
