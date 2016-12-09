#include "crete-debug.h"
#include "runtime-dump.h"

extern "C" {
#include "config.h"
#include "cpu.h"
}
#include "tcg.h"

#include <sys/time.h>
#include <sys/resource.h>

#include <map>
#include <vector>
#include <string>
#include <sstream>

#include "boost/unordered_set.hpp"

using namespace std;

#define CPU_OFFSET(field) offsetof(CPUArchState, field)

static boost::unordered_set<uint64_t> init_list_crete_dbg_tb_pc() {
    boost::unordered_set<uint64_t> list;

    list.insert(0x8c42f8e);

    return list;
}

static boost::unordered_set<uint64_t> init_list_crete_dbg_ta_guest_addr() {
    boost::unordered_set<uint64_t> list;

    list.insert(0xbffff5ef);

    return list;
}

const static boost::unordered_set<uint64_t> list_crete_dbg_tb_pc =
        init_list_crete_dbg_tb_pc();

const static boost::unordered_set<uint64_t> list_crete_dbg_ta_guest_addr =
        init_list_crete_dbg_ta_guest_addr();


int is_in_list_crete_dbg_tb_pc(uint64_t tb_pc) {
    if (list_crete_dbg_tb_pc.find(tb_pc) == list_crete_dbg_tb_pc.end())
    {
        return false;
    } else {
        return true;
    }
}

int is_in_list_crete_dbg_ta_guest_addr(uint64_t addr) {
    if (list_crete_dbg_ta_guest_addr.find(addr) ==
            list_crete_dbg_ta_guest_addr.end())
    {
        return false;
    } else {
        return true;
    }
}


// return the current memory usage in MB
float crete_mem_usage()
{
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

    return usage.ru_maxrss/1024.0;
}

static void init_x86_cpuState_reflection_info(map<uint64_t,
        pair<uint64, string> > &in_maps)
{
    /* standard registers */
    for(uint64_t i = 0; i < CPU_NB_REGS; ++i) {
        std::stringstream name;
        name << "regs_" << i;
        in_maps.insert(make_pair(CPU_OFFSET(regs) + i*sizeof(target_ulong),
                make_pair(sizeof(target_ulong), name.str())));

    }
    in_maps.insert(make_pair(CPU_OFFSET(eip),
            make_pair(sizeof(target_ulong), "eip")));
    in_maps.insert(make_pair(CPU_OFFSET(eflags),
            make_pair(sizeof(target_ulong), "eflags")));

    /* emulator internal eflags handling */
    in_maps.insert(make_pair(CPU_OFFSET(eflags),
            make_pair(sizeof(target_ulong), "eflags")));
    in_maps.insert(make_pair(CPU_OFFSET(cc_dst),
            make_pair(sizeof(target_ulong), "cc_dst")));
    in_maps.insert(make_pair(CPU_OFFSET(cc_src),
            make_pair(sizeof(target_ulong), "cc_src")));
    in_maps.insert(make_pair(CPU_OFFSET(cc_src2),
            make_pair(sizeof(target_ulong), "cc_src2")));
    in_maps.insert(make_pair(CPU_OFFSET(cc_op),
            make_pair(sizeof(uint32_t), "cc_op")));
    in_maps.insert(make_pair(CPU_OFFSET(df),
            make_pair(sizeof(int32_t), "df")));
    in_maps.insert(make_pair(CPU_OFFSET(hflags),
                make_pair(sizeof(uint32_t), "hflags")));
    in_maps.insert(make_pair(CPU_OFFSET(hflags2),
                make_pair(sizeof(uint32_t), "hflags2")));

    /* segments */
    /*
       SegmentCache segs[6];
       SegmentCache ldt;
       SegmentCache tr;
       SegmentCache gdt;
       SegmentCache idt;

       target_ulong cr[5];
       int32_t a20_mask;

       BNDReg bnd_regs[4];
       BNDCSReg bndcs_regs;
       uint64_t msr_bndcfgs;
       */

    /* Beginning of state preserved by INIT (dummy marker).  */
//    struct {} start_init_save;

    /* FPU state */
    in_maps.insert(make_pair(CPU_OFFSET(fpstt),
                make_pair(sizeof(unsigned int), "fpstt")));

    in_maps.insert(make_pair(CPU_OFFSET(fpus),
                make_pair(sizeof(uint16_t), "fpus")));

    in_maps.insert(make_pair(CPU_OFFSET(fpuc),
                make_pair(sizeof(uint16_t), "fpuc")));

    in_maps.insert(make_pair(CPU_OFFSET(fptags),
                make_pair(sizeof(uint8_t)*8, "fptags[8]")));

    in_maps.insert(make_pair(CPU_OFFSET(fpregs),
                make_pair(sizeof(FPReg) * 8, "fpregs[8]")));
    /* KVM-only so far */
//    uint16_t fpop;
//    uint64_t fpip;
//    uint64_t fpdp;

    /* emulator internal variables */
    in_maps.insert(make_pair(CPU_OFFSET(fp_status),
                make_pair(sizeof(float_status), "fp_status")));
    in_maps.insert(make_pair(CPU_OFFSET(ft0),
                make_pair(sizeof(floatx80), "ft0")));
    in_maps.insert(make_pair(CPU_OFFSET(mmx_status),
                make_pair(sizeof(float_status), "mmx_status")));
    in_maps.insert(make_pair(CPU_OFFSET(sse_status),
                make_pair(sizeof(float_status), "sse_status")));
    in_maps.insert(make_pair(CPU_OFFSET(mxcsr),
                make_pair(sizeof(uint32_t), "mxcsr")));
    in_maps.insert(make_pair(CPU_OFFSET(xmm_regs),
                    make_pair(sizeof(XMMReg) * (CPU_NB_REGS == 8 ? 8 : 32),
                            "xmm_regs[CPU_NB_REGS == 8 ? 8 : 32]")));
    in_maps.insert(make_pair(CPU_OFFSET(xmm_t0),
                    make_pair(sizeof(XMMReg), "xmm_t0")));
    in_maps.insert(make_pair(CPU_OFFSET(mmx_t0),
                    make_pair(sizeof(MMXReg), "mmx_t0")));
    in_maps.insert(make_pair(CPU_OFFSET(opmask_regs[NB_OPMASK_REGS]),
                make_pair(sizeof(uint64_t) * NB_OPMASK_REGS,
                        "opmask_regs[NB_OPMASK_REGS]")));

//    in_maps.insert(make_pair(CPU_OFFSET(),
//                    make_pair(sizeof(), "")));

    /* sysenter registers */
    //.........

}

void crete_print_x86_cpu_regs(uint64_t offset, uint64_t size, char* buf)
{
    // <offset, <size , name> >
    static map<uint64_t, pair<uint64, string> > x86_cpuState_reflection_info;

    if(x86_cpuState_reflection_info.empty())
    {
        init_x86_cpuState_reflection_info(x86_cpuState_reflection_info);
    }

    bool found = false;
    for(map<uint64_t, pair<uint64, string> >::const_iterator it
            = x86_cpuState_reflection_info.begin();
            it != x86_cpuState_reflection_info.end(); ++it) {
        if(offset == it->first)
        {
            sprintf(buf, "cpu->%s: %lu bytes",it->second.second.c_str(), size);
            found = true;
            break;
        }
    }

    if(!found) {
        sprintf(buf, "Not found: cpu(%lu, %lu)\n", offset, size);
    }
}

void print_x86_all_cpu_regs(void *cpu_state)
{
    CPUArchState *qemuCpuState = (CPUArchState *)cpu_state;
    cerr << "++++++++++++++++++++++++++\n"
            << "X86 CPU:\n";
    for(int i = 0; i < CPU_NB_REGS; ++i) {
        uint32_t reg_size = sizeof(target_ulong);

        fprintf(stderr,"reg[%d]", i);
        cerr << ": 0x" << hex << qemuCpuState->regs[i] << " [";
        for(uint32_t j = 0; j < reg_size; ++j) {
            cerr << hex << " 0x"<< (uint32_t) (*(((uint8_t *)qemuCpuState) + i*reg_size + j));
        }
        cerr << "]\n";
    }
    cerr << "++++++++++++++++++++++++++\n";
}

void crete_print_helper_function_name(uint64_t func_addr)
{
    string func_name = runtime_env->get_tcoHelper_name(func_addr);
    cerr << "helper function: " << func_name;
}

void dump_IR(void* void_s, unsigned long long ir_tb_pc) {
    TCGContext *s = (TCGContext *)void_s;

    char file_name[] = "tb-ir.txt";
    FILE *f = fopen(file_name, "a");
    assert(f);

    fprintf(f, "qemu-ir-tb-%llu-%p:\n",
            (unsigned long long )(nb_captured_llvm_tb - 1), (void *)(uint64_t)ir_tb_pc);
    tcg_dump_ops_file(s, f);
    fprintf(f, "\n");

    fclose(f);
}

void print_guest_memory(void *env_cpuState, uint64_t addr, int len)
{
    uint8_t* buf = new uint8_t[len];

    if(RuntimeEnv::access_guest_memory(env_cpuState, addr, buf, len, false) != 0){
        cerr << "[CRETE ERROR] access_guest_memory() failed within print_guest_memory()\n";
        assert(0);
    }

    fprintf(stderr, "guest_memory(%p, %d):[", (void *)addr, len);
    for(int i = 0; i < len; ++i){
        fprintf(stderr, " %p", (void *)(uint64_t)buf[i]);
    }

    cerr << "]\n";

    delete [] buf;
}


#define __CRETE_DEBUG_DUMP_CPU_STATE(in_type, in_name)                          \
        offset = CPU_OFFSET(in_name);                                           \
        size   = sizeof(in_type);                                               \
        name.str(string());                                                     \
        name << "debug_" << #in_name ;                                          \
        data.clear();                                                           \
                                                                                \
        for(uint64_t j=0; j < size; ++j) {                                      \
            data.push_back(*(uint8_t *)(((uint8_t *)target)                     \
                    + offset + j));                                             \
        }                                                                       \
                                                                                \
        ret.push_back(CPUStateElement(offset, size, name.str(), data));


#define __CRETE_DEBUG_DUMP_CPU_STATE_ARRAY(in_type, in_name, array_size)        \
        for(uint64_t i = 0; i < (array_size); ++i)                              \
        {                                                                       \
            offset = CPU_OFFSET(in_name) + i*sizeof(in_type);                   \
            size   = sizeof(in_type);                                           \
            name.str(string());                                                 \
            name << "debug_" << #in_name << "[" << dec << i << "]";             \
            data.clear();                                                       \
                                                                                \
            for(uint64_t j=0; j < size; ++j) {                                  \
                data.push_back(*(uint8_t *)(((uint8_t *)target)                 \
                        + offset + j));                                         \
            }                                                                   \
                                                                                \
            ret.push_back(CPUStateElement(offset, size, name.str(), data));     \
        }

vector<CPUStateElement> x86_cpuState_dump(const CPUArchState *target) {
    vector<CPUStateElement> ret;
    ret.clear();

    uint64_t offset;
    uint64_t size;
    stringstream name;
    vector<uint8_t> data;

    /* standard registers */
    // target_ulong regs[CPU_NB_REGS];
    __CRETE_DEBUG_DUMP_CPU_STATE_ARRAY(target_ulong, regs, CPU_NB_REGS)
//xxx: not traced
// target_ulong eip;
   // target_ulong eflags;
    __CRETE_DEBUG_DUMP_CPU_STATE(target_ulong, eflags)

    /* emulator internal eflags handling */
    // target_ulong cc_dst;
    __CRETE_DEBUG_DUMP_CPU_STATE(target_ulong, cc_dst)
    // target_ulong cc_src;
    __CRETE_DEBUG_DUMP_CPU_STATE(target_ulong, cc_src)
    // target_ulong cc_src2;
    __CRETE_DEBUG_DUMP_CPU_STATE(target_ulong, cc_src2)
    //uint32_t cc_op;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint32_t, cc_op);
    // int32_t df;
    __CRETE_DEBUG_DUMP_CPU_STATE(int32_t, df)
    // uint32_t hflags;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint32_t, hflags)
    // uint32_t hflags2;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint32_t, hflags2)

    /* segments */
    // SegmentCache segs[6];
    __CRETE_DEBUG_DUMP_CPU_STATE_ARRAY(SegmentCache, segs, 6)
    // SegmentCache ldt;
    __CRETE_DEBUG_DUMP_CPU_STATE(SegmentCache, ldt)
    // SegmentCache tr;
    __CRETE_DEBUG_DUMP_CPU_STATE(SegmentCache, tr)
    // SegmentCache gdt;
    __CRETE_DEBUG_DUMP_CPU_STATE(SegmentCache, gdt)
    // SegmentCache idt;
    __CRETE_DEBUG_DUMP_CPU_STATE(SegmentCache, idt)

// xxx: not traced
    // target_ulong cr[5];
//    __CRETE_DEBUG_DUMP_CPU_STATE_ARRAY(target_ulong, cr, 5)
    // int32_t a20_mask;
    __CRETE_DEBUG_DUMP_CPU_STATE(int32_t, a20_mask)

    // BNDReg bnd_regs[4];
    __CRETE_DEBUG_DUMP_CPU_STATE_ARRAY(BNDReg, bnd_regs, 4)
    // BNDCSReg bndcs_regs;
    __CRETE_DEBUG_DUMP_CPU_STATE(BNDCSReg, bndcs_regs)
    // uint64_t msr_bndcfgs;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, msr_bndcfgs)

    /* Beginning of state preserved by INIT (dummy marker).  */
// xxx: not traced
//    struct {} start_init_save;
//    __CRETE_DEBUG_DUMP_CPU_STATE(struct {}, start_init_save)

    /* FPU state */
    // unsigned int fpstt;
    __CRETE_DEBUG_DUMP_CPU_STATE(unsigned int, fpstt)
    // uint16_t fpus;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint16_t, fpus)
    // uint16_t fpuc;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint16_t, fpuc)
    // uint8_t fptags[8];
    __CRETE_DEBUG_DUMP_CPU_STATE_ARRAY(uint8_t, fptags, 8)
    // FPReg fpregs[8];
    __CRETE_DEBUG_DUMP_CPU_STATE_ARRAY(FPReg, fpregs, 8)
    /* KVM-only so far */
    // uint16_t fpop;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint16_t, fpop)
    // uint64_t fpip;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, fpip)
    // uint64_t fpdp;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, fpdp)

    /* emulator internal variables */
    // float_status fp_status;
    __CRETE_DEBUG_DUMP_CPU_STATE(float_status, fp_status)
    // floatx80 ft0;
    __CRETE_DEBUG_DUMP_CPU_STATE(floatx80, ft0)

    // float_status mmx_status;
    __CRETE_DEBUG_DUMP_CPU_STATE(float_status, mmx_status)
    // float_status sse_status;
    __CRETE_DEBUG_DUMP_CPU_STATE(float_status, sse_status)
    // uint32_t mxcsr;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint32_t, mxcsr)
    // XMMReg xmm_regs[CPU_NB_REGS == 8 ? 8 : 32];
    __CRETE_DEBUG_DUMP_CPU_STATE_ARRAY(XMMReg, xmm_regs, CPU_NB_REGS == 8 ? 8 : 32)
    // XMMReg xmm_t0;
    __CRETE_DEBUG_DUMP_CPU_STATE(XMMReg, xmm_t0)
    // MMXReg mmx_t0;
    __CRETE_DEBUG_DUMP_CPU_STATE(MMXReg, mmx_t0)

    // uint64_t opmask_regs[NB_OPMASK_REGS];
    __CRETE_DEBUG_DUMP_CPU_STATE_ARRAY(uint64_t, opmask_regs, NB_OPMASK_REGS)

    /* sysenter registers */
    // uint32_t sysenter_cs;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint32_t, sysenter_cs)
    // target_ulong sysenter_esp;
    __CRETE_DEBUG_DUMP_CPU_STATE(target_ulong, sysenter_esp)
    // target_ulong sysenter_eip;
    __CRETE_DEBUG_DUMP_CPU_STATE(target_ulong, sysenter_eip)
    // uint64_t efer;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, efer)
    // uint64_t star;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, star)

    // uint64_t vm_hsave;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, vm_hsave)

#ifdef TARGET_X86_64
    // target_ulong lstar;
    __CRETE_DEBUG_DUMP_CPU_STATE(target_ulong, lstar)
    // target_ulong cstar;
    __CRETE_DEBUG_DUMP_CPU_STATE(target_ulong, cstar)
    // target_ulong fmask;
    __CRETE_DEBUG_DUMP_CPU_STATE(target_ulong, fmask)
    // target_ulong kernelgsbase;
    __CRETE_DEBUG_DUMP_CPU_STATE(target_ulong, kernelgsbase)
#endif

    // uint64_t tsc;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, tsc)
    // uint64_t tsc_adjust;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, tsc_adjust)
    // uint64_t tsc_deadline;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, tsc_deadline)

    // uint64_t mcg_status;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, mcg_status)
    // uint64_t msr_ia32_misc_enable;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, msr_ia32_misc_enable)
    // uint64_t msr_ia32_feature_control;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, msr_ia32_feature_control)

    // uint64_t msr_fixed_ctr_ctrl;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, msr_fixed_ctr_ctrl)
    // uint64_t msr_global_ctrl;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, msr_global_ctrl)
    // uint64_t msr_global_status;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, msr_global_status)
    // uint64_t msr_global_ovf_ctrl;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, msr_global_ovf_ctrl)
    // uint64_t msr_fixed_counters[MAX_FIXED_COUNTERS];
    __CRETE_DEBUG_DUMP_CPU_STATE_ARRAY(uint64_t, msr_fixed_counters, MAX_FIXED_COUNTERS)
    // uint64_t msr_gp_counters[MAX_GP_COUNTERS];
    __CRETE_DEBUG_DUMP_CPU_STATE_ARRAY(uint64_t, msr_gp_counters, MAX_GP_COUNTERS)
    // uint64_t msr_gp_evtsel[MAX_GP_COUNTERS];
    __CRETE_DEBUG_DUMP_CPU_STATE_ARRAY(uint64_t, msr_gp_evtsel, MAX_GP_COUNTERS)

    // uint64_t pat;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, pat)
    // uint32_t smbase;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint32_t, smbase)

    /* End of state preserved by INIT (dummy marker).  */
// xxx: not traced
//    struct {} end_init_save;
//    __CRETE_DEBUG_DUMP_CPU_STATE(struct {}, end_init_save)

    // uint64_t system_time_msr;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, system_time_msr)
    // uint64_t wall_clock_msr;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, wall_clock_msr)
    // uint64_t steal_time_msr;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, steal_time_msr)
    // uint64_t async_pf_en_msr;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, async_pf_en_msr)
    // uint64_t pv_eoi_en_msr;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, pv_eoi_en_msr)

    // uint64_t msr_hv_hypercall;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, msr_hv_hypercall)
    // uint64_t msr_hv_guest_os_id;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, msr_hv_guest_os_id)
    // uint64_t msr_hv_vapic;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, msr_hv_vapic)
    // uint64_t msr_hv_tsc;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, msr_hv_tsc)

    /* exception/interrupt handling */
//xxx: not traced
// int error_code;
//    __CRETE_DEBUG_DUMP_CPU_STATE(int, error_code)
    // int exception_is_int;
    __CRETE_DEBUG_DUMP_CPU_STATE(int, exception_is_int)
    // target_ulong exception_next_eip;
    __CRETE_DEBUG_DUMP_CPU_STATE(target_ulong, exception_next_eip)
    // target_ulong dr[8];
    __CRETE_DEBUG_DUMP_CPU_STATE_ARRAY(target_ulong, dr, 8)
//xxx: not traced
//    union {
//        struct CPUBreakpoint *cpu_breakpoint[4];
//        struct CPUWatchpoint *cpu_watchpoint[4];
//    };
    // int old_exception;
    __CRETE_DEBUG_DUMP_CPU_STATE(int, old_exception)
    // uint64_t vm_vmcb;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, vm_vmcb)
    // uint64_t tsc_offset;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, tsc_offset)
    // uint64_t intercept;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint64_t, intercept)
    // uint16_t intercept_cr_read;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint16_t, intercept_cr_read)
    // uint16_t intercept_cr_write;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint16_t, intercept_cr_write)
    // uint16_t intercept_dr_read;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint16_t, intercept_dr_read)
    // uint16_t intercept_dr_write;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint16_t, intercept_dr_write)
    // uint32_t intercept_exceptions;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint32_t, intercept_exceptions)
    // uint8_t v_tpr;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint8_t, v_tpr)

    /* KVM states, automatically cleared on reset */
    // uint8_t nmi_injected;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint8_t, nmi_injected)
    // uint8_t nmi_pending;
    __CRETE_DEBUG_DUMP_CPU_STATE(uint8_t, nmi_pending)

//xxx: not traced
//    CPU_COMMON

    /* Fields from here on are preserved across CPU reset. */
/*
    uint32_t cpuid_level;
    uint32_t cpuid_xlevel;
    uint32_t cpuid_xlevel2;
    uint32_t cpuid_vendor1;
    uint32_t cpuid_vendor2;
    uint32_t cpuid_vendor3;
    uint32_t cpuid_version;
    FeatureWordArray features;
    uint32_t cpuid_model[12];

    uint64_t mtrr_fixed[11];
    uint64_t mtrr_deftype;
    MTRRVar mtrr_var[MSR_MTRRcap_VCNT];

    uint32_t mp_state;
    int32_t exception_injected;
    int32_t interrupt_injected;
    uint8_t soft_interrupt;
    uint8_t has_error_code;
    uint32_t sipi_vector;
    bool tsc_valid;
    int tsc_khz;
    void *kvm_xsave_buf;

    uint64_t mcg_cap;
    uint64_t mcg_ctl;
    uint64_t mce_banks[MCE_BANKS_DEF*4];

    uint64_t tsc_aux;

    uint16_t fpus_vmstate;
    uint16_t fptag_vmstate;
    uint16_t fpregs_format_vmstate;
    uint64_t xstate_bv;

    uint64_t xcr0;
    uint64_t xss;

    TPRAccess tpr_access_type;
*/

    return ret;
}

#if defined(CRETE_CROSS_CHECK)
// offset, size
static vector<pair<uint64_t, uint64_t> > c_cpuState_offset;
// name, offset, size
static vector<pair<string, pair<uint64_t, uint64_t> > > cxx_cpuState_offset;

void crete_add_c_cpuState_offset(uint64_t offset, uint64_t size)
{
    c_cpuState_offset.push_back(make_pair(offset, size));
}

#define __ADD_CXX_CPUSTATE_OFFSET(in_type, in_name)         \
        name.str(string());                                                 \
        name << #in_name;             \
        offset = offsetof(CPUArchState, in_name);        \
        size = sizeof(in_type);                          \
        cxx_cpuState_offset.push_back(make_pair(name.str(),             \
                make_pair(offset, size)));

#define __ADD_CXX_CPUSTATE_OFFSET_ARRAY(in_type, in_name, array_size)       \
        for(i = 0; i < (array_size); ++i)                                   \
        {                                                                   \
            name.str(string());                                             \
            name << #in_name << "[" << dec << i << "]";                     \
            offset = offsetof(CPUArchState, in_name) + i*sizeof(in_type);   \
            size = sizeof(in_type);                                         \
            cxx_cpuState_offset.push_back(make_pair(name.str(),             \
                    make_pair(offset, size)));                              \
        }

void init_CXX_CPUArchState_offset(void)
{
    uint64_t i = 0;
    stringstream name;
    uint64_t offset = 0;
    uint64_t size = 0;

    __ADD_CXX_CPUSTATE_OFFSET(target_ulong, regs);

    __ADD_CXX_CPUSTATE_OFFSET(target_ulong, eflags)

    /* emulator internal eflags handling */
    // target_ulong cc_dst;
    __ADD_CXX_CPUSTATE_OFFSET(target_ulong, cc_dst)
    // target_ulong cc_src;
    __ADD_CXX_CPUSTATE_OFFSET(target_ulong, cc_src)
    // target_ulong cc_src2;
    __ADD_CXX_CPUSTATE_OFFSET(target_ulong, cc_src2)
    //uint32_t cc_op;
    __ADD_CXX_CPUSTATE_OFFSET(uint32_t, cc_op);
    // int32_t df;
    __ADD_CXX_CPUSTATE_OFFSET(int32_t, df)
    // uint32_t hflags;
    __ADD_CXX_CPUSTATE_OFFSET(uint32_t, hflags)
    // uint32_t hflags2;
    __ADD_CXX_CPUSTATE_OFFSET(uint32_t, hflags2)

    /* segments */
    // SegmentCache segs[6];
    __ADD_CXX_CPUSTATE_OFFSET_ARRAY(SegmentCache, segs, 6)

    // SegmentCache ldt;
    __ADD_CXX_CPUSTATE_OFFSET(SegmentCache, ldt)
    // SegmentCache tr;
    __ADD_CXX_CPUSTATE_OFFSET(SegmentCache, tr)
    // SegmentCache gdt;
    __ADD_CXX_CPUSTATE_OFFSET(SegmentCache, gdt)
    // SegmentCache idt;
    __ADD_CXX_CPUSTATE_OFFSET(SegmentCache, idt)

    // xxx: not traced
    // target_ulong cr[5];
    //    __ADD_CXX_CPUSTATE_OFFSET_ARRAY(target_ulong, cr, 5)
    // int32_t a20_mask;
    __ADD_CXX_CPUSTATE_OFFSET(int32_t, a20_mask)

    // BNDReg bnd_regs[4];
    __ADD_CXX_CPUSTATE_OFFSET_ARRAY(BNDReg, bnd_regs, 4)
    // BNDCSReg bndcs_regs;
    __ADD_CXX_CPUSTATE_OFFSET(BNDCSReg, bndcs_regs)
    // uint64_t msr_bndcfgs;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, msr_bndcfgs)

    /* Beginning of state preserved by INIT (dummy marker).  */
    // xxx: not traced
    //    struct {} start_init_save;
    //    __ADD_CXX_CPUSTATE_OFFSET(struct {}, start_init_save)

    /* FPU state */
    // unsigned int fpstt;
    __ADD_CXX_CPUSTATE_OFFSET(unsigned int, fpstt)
    // uint16_t fpus;
    __ADD_CXX_CPUSTATE_OFFSET(uint16_t, fpus)
    // uint16_t fpuc;
    __ADD_CXX_CPUSTATE_OFFSET(uint16_t, fpuc)
    // uint8_t fptags[8];
    __ADD_CXX_CPUSTATE_OFFSET_ARRAY(uint8_t, fptags, 8)
    // FPReg fpregs[8];
    __ADD_CXX_CPUSTATE_OFFSET_ARRAY(FPReg, fpregs, 8)
    /* KVM-only so far */
    // uint16_t fpop;
    __ADD_CXX_CPUSTATE_OFFSET(uint16_t, fpop)
    // uint64_t fpip;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, fpip)
    // uint64_t fpdp;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, fpdp)

    /* emulator internal variables */
    // float_status fp_status;
    __ADD_CXX_CPUSTATE_OFFSET(float_status, fp_status)
    // floatx80 ft0;
    __ADD_CXX_CPUSTATE_OFFSET(floatx80, ft0)

    // float_status mmx_status;
    __ADD_CXX_CPUSTATE_OFFSET(float_status, mmx_status)
    // float_status sse_status;
    __ADD_CXX_CPUSTATE_OFFSET(float_status, sse_status)
    // uint32_t mxcsr;
    __ADD_CXX_CPUSTATE_OFFSET(uint32_t, mxcsr)
    // XMMReg xmm_regs[CPU_NB_REGS == 8 ? 8 : 32];
    __ADD_CXX_CPUSTATE_OFFSET_ARRAY(XMMReg, xmm_regs, CPU_NB_REGS == 8 ? 8 : 32)
    // XMMReg xmm_t0;
    __ADD_CXX_CPUSTATE_OFFSET(XMMReg, xmm_t0)
    // MMXReg mmx_t0;
    __ADD_CXX_CPUSTATE_OFFSET(MMXReg, mmx_t0)

    // uint64_t opmask_regs[NB_OPMASK_REGS];
    __ADD_CXX_CPUSTATE_OFFSET_ARRAY(uint64_t, opmask_regs, NB_OPMASK_REGS)

    /* sysenter registers */
    // uint32_t sysenter_cs;
    __ADD_CXX_CPUSTATE_OFFSET(uint32_t, sysenter_cs)
    // target_ulong sysenter_esp;
    __ADD_CXX_CPUSTATE_OFFSET(target_ulong, sysenter_esp)
    // target_ulong sysenter_eip;
    __ADD_CXX_CPUSTATE_OFFSET(target_ulong, sysenter_eip)
    // uint64_t efer;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, efer)
    // uint64_t star;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, star)

    // uint64_t vm_hsave;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, vm_hsave)

#ifdef TARGET_X86_64
    // target_ulong lstar;
    __ADD_CXX_CPUSTATE_OFFSET(target_ulong, lstar)
    // target_ulong cstar;
    __ADD_CXX_CPUSTATE_OFFSET(target_ulong, cstar)
    // target_ulong fmask;
    __ADD_CXX_CPUSTATE_OFFSET(target_ulong, fmask)
    // target_ulong kernelgsbase;
    __ADD_CXX_CPUSTATE_OFFSET(target_ulong, kernelgsbase)
#endif

    // uint64_t tsc;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, tsc)
    // uint64_t tsc_adjust;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, tsc_adjust)
    // uint64_t tsc_deadline;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, tsc_deadline)

    // uint64_t mcg_status;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, mcg_status)
    // uint64_t msr_ia32_misc_enable;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, msr_ia32_misc_enable)
    // uint64_t msr_ia32_feature_control;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, msr_ia32_feature_control)

    // uint64_t msr_fixed_ctr_ctrl;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, msr_fixed_ctr_ctrl)
    // uint64_t msr_global_ctrl;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, msr_global_ctrl)
    // uint64_t msr_global_status;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, msr_global_status)
    // uint64_t msr_global_ovf_ctrl;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, msr_global_ovf_ctrl)
    // uint64_t msr_fixed_counters[MAX_FIXED_COUNTERS];
    __ADD_CXX_CPUSTATE_OFFSET_ARRAY(uint64_t, msr_fixed_counters, MAX_FIXED_COUNTERS)
    // uint64_t msr_gp_counters[MAX_GP_COUNTERS];
    __ADD_CXX_CPUSTATE_OFFSET_ARRAY(uint64_t, msr_gp_counters, MAX_GP_COUNTERS)
    // uint64_t msr_gp_evtsel[MAX_GP_COUNTERS];
    __ADD_CXX_CPUSTATE_OFFSET_ARRAY(uint64_t, msr_gp_evtsel, MAX_GP_COUNTERS)

    // uint64_t pat;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, pat)
    // uint32_t smbase;
    __ADD_CXX_CPUSTATE_OFFSET(uint32_t, smbase)

    /* End of state preserved by INIT (dummy marker).  */
    // xxx: not traced
    //    struct {} end_init_save;
    //    __ADD_CXX_CPUSTATE_OFFSET(struct {}, end_init_save)

    // uint64_t system_time_msr;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, system_time_msr)
    // uint64_t wall_clock_msr;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, wall_clock_msr)
    // uint64_t steal_time_msr;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, steal_time_msr)
    // uint64_t async_pf_en_msr;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, async_pf_en_msr)
    // uint64_t pv_eoi_en_msr;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, pv_eoi_en_msr)

    // uint64_t msr_hv_hypercall;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, msr_hv_hypercall)
    // uint64_t msr_hv_guest_os_id;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, msr_hv_guest_os_id)
    // uint64_t msr_hv_vapic;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, msr_hv_vapic)
    // uint64_t msr_hv_tsc;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, msr_hv_tsc)

    /* exception/interrupt handling */
    //xxx: not traced
    // int error_code;
    //    __ADD_CXX_CPUSTATE_OFFSET(int, error_code)
    // int exception_is_int;
    __ADD_CXX_CPUSTATE_OFFSET(int, exception_is_int)
    // target_ulong exception_next_eip;
    __ADD_CXX_CPUSTATE_OFFSET(target_ulong, exception_next_eip)
    // target_ulong dr[8];
    __ADD_CXX_CPUSTATE_OFFSET_ARRAY(target_ulong, dr, 8)
    //xxx: not traced
    //    union {
    //        struct CPUBreakpoint *cpu_breakpoint[4];
    //        struct CPUWatchpoint *cpu_watchpoint[4];
    //    };
    // int old_exception;
    __ADD_CXX_CPUSTATE_OFFSET(int, old_exception)
    // uint64_t vm_vmcb;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, vm_vmcb)
    // uint64_t tsc_offset;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, tsc_offset)
    // uint64_t intercept;
    __ADD_CXX_CPUSTATE_OFFSET(uint64_t, intercept)
    // uint16_t intercept_cr_read;
    __ADD_CXX_CPUSTATE_OFFSET(uint16_t, intercept_cr_read)
    // uint16_t intercept_cr_write;
    __ADD_CXX_CPUSTATE_OFFSET(uint16_t, intercept_cr_write)
    // uint16_t intercept_dr_read;
    __ADD_CXX_CPUSTATE_OFFSET(uint16_t, intercept_dr_read)
    // uint16_t intercept_dr_write;
    __ADD_CXX_CPUSTATE_OFFSET(uint16_t, intercept_dr_write)
    // uint32_t intercept_exceptions;
    __ADD_CXX_CPUSTATE_OFFSET(uint32_t, intercept_exceptions)
    // uint8_t v_tpr;
    __ADD_CXX_CPUSTATE_OFFSET(uint8_t, v_tpr)

    /* KVM states, automatically cleared on reset */
    // uint8_t nmi_injected;
    __ADD_CXX_CPUSTATE_OFFSET(uint8_t, nmi_injected)
    // uint8_t nmi_pending;
    __ADD_CXX_CPUSTATE_OFFSET(uint8_t, nmi_pending)
}

extern "C" {
void init_C_CPUArchState_offset(void);
}

void crete_verify_cpuState_offset_c_cxx()
{
    cxx_cpuState_offset.clear();
    c_cpuState_offset.clear();

    init_C_CPUArchState_offset();
    init_CXX_CPUArchState_offset();

    assert(c_cpuState_offset.size() == cxx_cpuState_offset.size());

    bool matched = true;
    for(uint64_t i = 0; i < cxx_cpuState_offset.size(); ++i) {
        if((cxx_cpuState_offset[i].second.first != c_cpuState_offset[i].first) ||
                (cxx_cpuState_offset[i].second.second!= c_cpuState_offset[i].second)){
            string name = cxx_cpuState_offset[i].first;
            uint64_t c_offset = c_cpuState_offset[i].first;
            uint64_t c_size= c_cpuState_offset[i].second;

            uint64_t cxx_offset = cxx_cpuState_offset[i].second.first;
            uint64_t cxx_size= cxx_cpuState_offset[i].second.second;

            fprintf(stderr, "[CRETE ERROR] mismatched on %s:\tc[%lu, %lu], cxx[%lu, %lu]\n",
                    name.c_str(), c_offset, c_size, cxx_offset, cxx_size);

            matched = false;
        }
    }

    assert(matched);
    assert(runtime_env);
    runtime_env->init_debug_cpuState_offsets(cxx_cpuState_offset);
}
#endif //#if defined(CRETE_CROSS_CHECK)

