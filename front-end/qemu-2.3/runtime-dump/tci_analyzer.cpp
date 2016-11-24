#include "tci_analyzer.h"

extern "C" {
#include "config.h"
#include "cpu.h"
#include "tcg-target.h"
}

#include <boost/array.hpp>
#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>

#include <iostream>
#include <fstream>
#include <sstream>

#include "crete-debug.h"

static const uint64_t CRETE_TCG_ENV_SIZE = sizeof(CPUArchState);

extern "C" {
void crete_print_x86_cpu_regs(uint64_t offset, uint64_t size, char* buf);
}

namespace crete
{
namespace tci
{

struct Block
{
    Block()
        : symbolic_block_(false) {}

    bool symbolic_block_;
};

struct vCPUReg{
    uint64_t m_offset;
    uint64_t m_size;
    std::string m_name;

    vCPUReg(uint64_t offset, uint64_t size, std::string name)
    :m_offset(offset), m_size(size), m_name(name) {}
};

/* Pitfalls:
 * 1. guest_vcpu_regs_: TA and CPUState tracing monitors different part of CPU State,
 *      a) they have different blacklist:
 *         1. What's the implication of this inconsistency?
 *         2. Are the black-lists sound?
 *         3. Any potential of under/over tainting?
 *      b) crete_update_tainted_vcpu_regs():
 *         1. Operates on the granularity of register. Will byte-wise operation be better?
 *         2. Any potential of under/over tainting?
 * */

class Analyzer
{
public:
    // The address of tainted memory byte
    typedef boost::unordered_set<uint64_t> MemSet;
    // virtual guest address, value
    typedef boost::unordered_map<uint64_t, uint8_t> taintedMem_ty;
    // (offset, vCPUReg)
    typedef boost::unordered_map<uint64_t, vCPUReg> cpuRegsTable_ty;
public:
    Analyzer();
    void init(uint64_t guest_vcpu_addr, uint64_t tcg_sp_value);
    void terminate_block();

    void reset_tcg_reg();
    bool is_tcg_reg_symbolic(uint64_t index, uint64_t data);
    void make_tcg_reg_symbolic(uint64_t index, uint64_t data);
    void make_tcg_reg_concrete(uint64_t index, uint64_t data);

    bool is_guest_mem_symbolic(uint64_t addr, uint64_t size, uint64_t data);
    void make_guest_mem_symbolic(uint64_t addr, uint64_t size, uint64_t data);
    void make_guest_mem_concrete(uint64_t addr, uint64_t size, uint64_t data);

    bool is_host_mem_symbolic(uint64_t base_addr, uint64_t offset, uint64_t size);
    void make_host_mem_symbolic(uint64_t base_addr, uint64_t offset, uint64_t size);
    void make_host_mem_concrete(uint64_t base_addr, uint64_t offset, uint64_t size);

    bool is_within_vcpu(uint64_t addr, uint64_t size);

    bool is_block_symbolic();
    bool is_previous_block_symbolic();
    void mark_block_symbolic();

    static cpuRegsTable_ty init_guest_vcpu_regs_black_list();
    static cpuRegsTable_ty init_guest_vcpu_regs_table();

    // Debug
    void dbg_print();

private:
    Block current_block_;

    taintedMem_ty guest_mem_;

    // <tainted, value>
    std::pair<bool, uint8_t> guest_vcpu_regs_[CRETE_TCG_ENV_SIZE];
    uint64_t guest_vcpu_addr_; // The address of the guest virtual cpu

    // <tainted, value>
    std::pair<bool, uint64_t> tcg_regs_[TCG_TARGET_NB_REGS];

    // tcg_sp: the value of call stack pointer used by tcg
    // this should be the only host register address that being used by instructions op_st/ld, besides guest_vcpu_reg
    uint64_t tcg_sp_value_;
    taintedMem_ty tcg_call_stack_mem_;

    bool previous_block_symbolic_; // Denotes that the previous block was symbolic.
    bool initialized_;

    static const cpuRegsTable_ty guest_vcpu_regs_black_list_;
    static const cpuRegsTable_ty guest_vcpu_regs_table_;
};

Analyzer::Analyzer()
    : guest_vcpu_addr_(0)
    , tcg_sp_value_(0)
    , previous_block_symbolic_(false)
    , initialized_(false)
{
}

void Analyzer::init(uint64_t guest_vcpu_addr, uint64_t tcg_sp_value)
{
    if(initialized_)
        return;

    guest_vcpu_addr_ = guest_vcpu_addr;
    tcg_sp_value_ = tcg_sp_value;

    initialized_  = true;
}

void Analyzer::terminate_block()
{
    previous_block_symbolic_ = current_block_.symbolic_block_;

    current_block_ = Block();

    // TODO: xxx double check with reviewing qemu's code on
    //      the assumption that tcg_regs are not shared across
    //      different TBs
    reset_tcg_reg();
}

void Analyzer::reset_tcg_reg()
{
    for(uint64_t i = 0; i < TCG_TARGET_NB_REGS; ++i)
    {
        tcg_regs_[i].first = false;
    }
}

bool Analyzer::is_tcg_reg_symbolic(uint64_t index, uint64_t data)
{
    assert(index < TCG_TARGET_NB_REGS);

    bool ret = false;
    if(tcg_regs_[index].first){
        if(tcg_regs_[index].second == data){
            ret = true;

#if defined(CRETE_DBG_TA)
            fprintf(stderr, "[CRETE Info] TA: tcg_regs_[%lu] is symbolic.\n", index);
#endif
        } else {
            tcg_regs_[index].first = false;

            CRETE_DBG_GEN(
            fprintf(stderr, "[CRETE Warning] TA: is_tcg_reg_symbolic() "
                    "potential under-taint-analysis: tcg_regs_[%lu] is changed "
                    "while is tainted.\n", index);
            );
        }
    }

    return ret;
}

void Analyzer::make_tcg_reg_symbolic(uint64_t index, uint64_t data)
{
    assert(index < TCG_TARGET_NB_REGS);
    tcg_regs_[index].first = true;
    tcg_regs_[index].second = data;

    mark_block_symbolic();

#if defined(CRETE_DBG_TA)
    fprintf(stderr, "[CRETE Info] TA: make_tcg_reg_symbolic(): tcg_regs_[%lu]\n", index);
#endif
}

void Analyzer::make_tcg_reg_concrete(uint64_t index, uint64_t data)
{
    assert(index < TCG_TARGET_NB_REGS);
    tcg_regs_[index].first = false;

#if defined(CRETE_DBG_TA)
    fprintf(stderr, "[CRETE Info] TA: make_tcg_reg_concrete(): tcg_regs_[%lu]\n", index);
#endif
}

bool Analyzer::is_guest_mem_symbolic(uint64_t addr, uint64_t size, uint64_t data)
{
    bool ret = false;

    taintedMem_ty::iterator it;
    uint8_t byte_value = 0;
    for(uint64_t i = 0; i < size; ++i) {
        it = guest_mem_.find(addr + i);
        if(it == guest_mem_.end())
            continue;

        byte_value = (data >> i*8) & 0xff;
        if(it->second == byte_value) {
            ret = true;

#if defined(CRETE_DBG_TA)
            if(is_in_list_crete_dbg_ta_guest_addr(addr+i))
                fprintf(stderr, "is_guest_mem_symbolic() is true for address %p, value = %d\n",
                        (void *)(addr+i), guest_mem_[addr+i]);
#endif
        } else {
            CRETE_DBG_GEN(
            fprintf(stderr, "[CRETE Warning] TA: is_guest_mem_symbolic() "
                    "potential under-taint-analysis: (%p) is changed (from %d to %d)"
                    "while is tainted.\n", (void *)(addr + i), guest_mem_[addr+i], byte_value);
            );

            guest_mem_.erase(it);
        }
    }

    return ret;
}

void Analyzer::make_guest_mem_symbolic(uint64_t addr, uint64_t size, uint64_t data)
{
    for(uint64_t i = 0; i < size; ++i) {
        guest_mem_[addr+i] = (data >> i*8) & 0xff;

#if defined(CRETE_DBG_TA)
        if(is_in_list_crete_dbg_ta_guest_addr(addr+i))
            fprintf(stderr, "make_guest_mem_symbolic() for address %p, value = %d\n",
                    (void *)(addr+i), (int)guest_mem_[addr+i]);
#endif
    }

    mark_block_symbolic();
}

void Analyzer::make_guest_mem_concrete(uint64_t addr, uint64_t size, uint64_t data)
{
    for(uint64_t i = 0; i < size; ++i) {
        guest_mem_.erase(addr + i);

#if defined(CRETE_DBG_TA)
        if(is_in_list_crete_dbg_ta_guest_addr(addr+i))
            fprintf(stderr, "make_guest_mem_concrete() for address %p\n",
                    (void *)(addr+i));
#endif
    }
}

bool Analyzer::is_block_symbolic()
{
    return current_block_.symbolic_block_;
}

bool Analyzer::is_previous_block_symbolic()
{
    return previous_block_symbolic_;
}

void Analyzer::mark_block_symbolic()
{
    CRETE_DBG_GEN(
    fprintf(stderr, "Analyzer::mark_block_symbolic()\n");
    );

    current_block_.symbolic_block_ = true;
}

bool Analyzer::is_host_mem_symbolic(uint64_t base_addr, uint64_t offset, uint64_t size)
{
    bool ret = false;

    if(base_addr == guest_vcpu_addr_){
        // Special case for offset "-4"
        if(offset == (uint64_t)(-4)) {
            return false;
        }

        if((offset >> 63) & 1){
            fprintf(stderr, "base_addr = %p, offset = %p, size = %lu\n",
                    (void *)base_addr, (void *)offset, size);

            assert( 0 && "[CRETE ERROR] when base addr is guest_vcpu_addr_, "
                    "its offset should always be positive.\n ");
        }

        assert( (offset + size -1) < CRETE_TCG_ENV_SIZE);
        const uint8_t *current_cpuState = (const uint8_t *)guest_vcpu_addr_;
        for(uint64_t i = 0; i < size; ++i){
            if(!guest_vcpu_regs_[offset + i].first)
                continue;

            if(guest_vcpu_regs_[offset + i].second == current_cpuState[offset + i]) {
                ret = true;

#if defined(CRETE_DBG_TA)
                fprintf(stderr, "[CRETE Info] TA: guest_vcpu_regs_ is symbolic: offset %lu\n",
                        offset + i);
#endif

            } else {
                guest_vcpu_regs_[offset + i].first = false;

                CRETE_DBG_GEN(
                fprintf(stderr, "[CRETE Warning] TA: vcpu in is_host_mem_symbolic() "
                        "potential under-taint-analysis: offset = %lu is changed while is tainted.\n",
                        (offset + i));
                );
            }
        }

    } else if (base_addr == tcg_sp_value_) {
        assert(((offset >> 63) & 1)  && "[CRETE ERROR] when base addr is tcg_sp_value_, "
                "its offset should always be negative.\n ");

        taintedMem_ty::iterator it;
        uint8_t byte_value;
        for(uint64_t i = 0; i < size; ++i) {
            it = tcg_call_stack_mem_.find(offset + i);
            if(it == tcg_call_stack_mem_.end())
                continue;

            byte_value = *(uint8_t *)(tcg_sp_value_ + offset + i);
            if(it->second == byte_value) {
                ret = true;

#if defined(CRETE_DBG_TA)
                fprintf(stderr, "[CRETE Info] TA: tcg_sp_value_ is symbolic: offset %lu\n",
                        offset + i);
#endif
            } else {
                tcg_call_stack_mem_.erase(it);

                CRETE_DBG_GEN(
                fprintf(stderr, "[CRETE Warning] TA: tcg_call_stack_mem_ in is_host_mem_symbolic() "
                        "potential under-taint-analysis: (%ld) is changed "
                        "while is tainted.\n", (int64_t)(offset + i));
                );
            }
        }
    } else {
        assert(0 && "[CRETE ERROR] base_addr is neither guest_vcpu_addr_ nor tcg_sp_value_\n");
    }

    return ret;
}

void Analyzer::make_host_mem_symbolic(uint64_t base_addr, uint64_t offset, uint64_t size)
{
    if(base_addr == guest_vcpu_addr_) {
        // Special case for offset "-4"
        if(offset == (uint64_t)(-4)) {
            fprintf(stderr, "[CRETE Warning] TA: make_host_mem_symbolic(): "
                    " try to taint special offset -4 from guest_vcpu_addr.\n");

            return;
        }

        if((offset >> 63) & 1){
            fprintf(stderr, "base_addr = %p, offset = %p, size = %lu\n",
                    (void *)base_addr, (void *)offset, size);

            assert( 0 && "[CRETE ERROR] when base addr is guest_vcpu_addr_, "
                    "its offset should always be positive.\n ");
        }

        // Check against black-listed vCpu regs
        cpuRegsTable_ty::const_iterator bk_it =
                guest_vcpu_regs_black_list_.find(offset);
        if(bk_it != guest_vcpu_regs_black_list_.end()){
            //        fprintf(stderr, "[CRETE DBG] Skip a black-listed cpu reg\n");
            assert(bk_it->second.m_size == size);
            return;
        }

        assert( (offset + size -1) < CRETE_TCG_ENV_SIZE);
        const uint8_t *current_cpuState = (const uint8_t *)guest_vcpu_addr_;
        for(uint64_t i = 0; i < size; ++i){
            guest_vcpu_regs_[offset + i].second = current_cpuState[offset + i];
            guest_vcpu_regs_[offset + i].first = true;
        }
    } else if (base_addr == tcg_sp_value_) {
        assert(((offset >> 63) & 1)  && "[CRETE ERROR] when base addr is not vcpu, "
                "its offset should always be negative.\n ");

        for(uint64_t i = 0; i < size ; ++i) {
            tcg_call_stack_mem_[offset + i] = *(uint8_t *)(tcg_sp_value_ + offset + i);
        }
    } else {
        assert(0 && "[CRETE ERROR] base_addr is neither guest_vcpu_addr_ nor tcg_sp_value_\n");
    }

    mark_block_symbolic();

#if defined(CRETE_DBG_TA)
    fprintf(stderr, "make_host_mem_symbolic()\n");
#endif
}

void Analyzer::make_host_mem_concrete(uint64_t base_addr, uint64_t offset, uint64_t size)
{
    if(base_addr == guest_vcpu_addr_){
        // Special case for offset "-4"
        if(offset == (uint64_t)(-4)) {
            return;
        }

        if((offset >> 63) & 1){
            fprintf(stderr, "base_addr = %p, offset = %p, size = %lu\n",
                    (void *)base_addr, (void *)offset, size);

            assert( 0 && "[CRETE ERROR] when base addr is guest_vcpu_addr_, "
                    "its offset should always be positive.\n ");
        }

        assert( (offset + size -1) < CRETE_TCG_ENV_SIZE);
        for(uint64_t i = 0; i < size; ++i)
            guest_vcpu_regs_[offset + i].first = false;
    } else if (base_addr == tcg_sp_value_) {
        assert(((offset >> 63) & 1)  && "[CRETE ERROR] when base addr is not vcpu, "
                "its offset should always be negative.\n ");

        for(uint64_t i = 0; i < size ; ++i) {
            tcg_call_stack_mem_.erase(offset + i);
        }
    } else {
        assert(0 && "[CRETE ERROR] base_addr is neither guest_vcpu_addr_ nor tcg_sp_value_\n");
    }
}


bool Analyzer::is_within_vcpu(uint64_t addr, uint64_t size)
{
    if(guest_vcpu_addr_ == 0)
        return false;

    if(addr >= guest_vcpu_addr_ &&
            (addr + size <= guest_vcpu_addr_ + CRETE_TCG_ENV_SIZE))
        return true;
    else
        return false;
}

void Analyzer::dbg_print()
{
    for(taintedMem_ty::const_iterator it = guest_mem_.begin();
    it != guest_mem_.end();
    ++it)
    {
        std::cerr << "tained guest mem: "
        << std::hex
        << it->first
        << std::dec
        << std::endl;
    }

//    for(boost::array<Reg, register_count>::iterator it = reg_.begin();
//            it != reg_.end();
//            ++it)
//    {
//        if(it->symbolic)
//        {
//            std::cerr << "reg "
//                    << std::distance(reg_.begin(), it)
//            << std::endl;
//        }
//    }
//
    for(uint64_t i = 0; i < CRETE_TCG_ENV_SIZE; ++i)
    {
        if(guest_vcpu_regs_[i].first)
            fprintf(stderr, "tainted vcpu byte: 0x%p\n", (void *)i);
    }
}

#define __CRETE_ADD_VCPU_REG_PAIR(in_map, in_type, in_name)                        \
        offset = offsetof(CPUArchState, in_name);                                  \
        size   = sizeof(in_type);                                                  \
        name.str(std::string());                                                   \
        name << #in_name;                                                          \
        in_map.insert(std::make_pair(offset, vCPUReg(offset, size, name.str())));

#define __CRETE_ADD_VCPU_REG_PAIR_ARRAY(in_map, in_type, in_name, array_size)      \
        for(uint64_t i = 0; i < (array_size); ++i)                                 \
        {                                                                          \
            offset = offsetof(CPUArchState, in_name);                              \
            size   = sizeof(in_type);                                              \
            name.str(std::string());                                               \
            name << #in_name << "[" << std::dec << i << "]";                       \
            in_map.insert(std::make_pair(offset, vCPUReg(offset, size, name.str()))); \
        }

// Those cc_* are black-listed as they are only used by qemu for helping emulation
//(not for emulating states of a real CPUState), and hence should not be traced
Analyzer::cpuRegsTable_ty Analyzer::init_guest_vcpu_regs_black_list()
{
    cpuRegsTable_ty ret;
    uint64_t offset;
    uint64_t size;
    std::stringstream name;

    __CRETE_ADD_VCPU_REG_PAIR(ret, target_ulong, cc_dst)
    __CRETE_ADD_VCPU_REG_PAIR(ret, target_ulong, cc_src)
    __CRETE_ADD_VCPU_REG_PAIR(ret, target_ulong, cc_src2)
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint32_t, cc_op)
    __CRETE_ADD_VCPU_REG_PAIR(ret, int32_t, df)
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint32_t, hflags)
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint32_t, hflags2)

    assert(ret.size() == 7);

    return ret;
}

Analyzer::cpuRegsTable_ty Analyzer::init_guest_vcpu_regs_table()
{
    cpuRegsTable_ty ret;
    uint64_t offset;
    uint64_t size;
    std::stringstream name;

    /* standard registers */
    // target_ulong regs[CPU_NB_REGS];
    __CRETE_ADD_VCPU_REG_PAIR_ARRAY(ret, target_ulong, regs, CPU_NB_REGS)

//xxx: not traced
// target_ulong eip;

   // target_ulong eflags;
    __CRETE_ADD_VCPU_REG_PAIR(ret, target_ulong, eflags)

    /* emulator internal eflags handling */
    // target_ulong cc_dst;
    __CRETE_ADD_VCPU_REG_PAIR(ret, target_ulong, cc_dst)
    // target_ulong cc_src;
    __CRETE_ADD_VCPU_REG_PAIR(ret, target_ulong, cc_src)
    // target_ulong cc_src2;
    __CRETE_ADD_VCPU_REG_PAIR(ret, target_ulong, cc_src2)
    //uint32_t cc_op;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint32_t, cc_op);

    // int32_t df;
    __CRETE_ADD_VCPU_REG_PAIR(ret, int32_t, df)
    // uint32_t hflags;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint32_t, hflags)
    // uint32_t hflags2;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint32_t, hflags2)

    /* segments */
    // SegmentCache segs[6];
    __CRETE_ADD_VCPU_REG_PAIR_ARRAY(ret, SegmentCache, segs, 6)
    // SegmentCache ldt;
    __CRETE_ADD_VCPU_REG_PAIR(ret, SegmentCache, ldt)
    // SegmentCache tr;
    __CRETE_ADD_VCPU_REG_PAIR(ret, SegmentCache, tr)
    // SegmentCache gdt;
    __CRETE_ADD_VCPU_REG_PAIR(ret, SegmentCache, gdt)
    // SegmentCache idt;
    __CRETE_ADD_VCPU_REG_PAIR(ret, SegmentCache, idt)

// xxx: not traced
// target_ulong cr[5];
//    __CRETE_ADD_VCPU_REG_PAIR_ARRAY(ret, target_ulong, cr, 5)

    // int32_t a20_mask;
    __CRETE_ADD_VCPU_REG_PAIR(ret, int32_t, a20_mask)

    // BNDReg bnd_regs[4];
    __CRETE_ADD_VCPU_REG_PAIR_ARRAY(ret, BNDReg, bnd_regs, 4)
    // BNDCSReg bndcs_regs;
    __CRETE_ADD_VCPU_REG_PAIR(ret, BNDCSReg, bndcs_regs)
    // uint64_t msr_bndcfgs;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, msr_bndcfgs)

    /* Beginning of state preserved by INIT (dummy marker).  */
//xxx: not traced
//    struct {} start_init_save;
//    __CRETE_ADD_VCPU_REG_PAIR(ret, struct {}, start_init_save)

    /* FPU state */
    // unsigned int fpstt;
    __CRETE_ADD_VCPU_REG_PAIR(ret, unsigned int, fpstt)
    // uint16_t fpus;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint16_t, fpus)
    // uint16_t fpuc;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint16_t, fpuc)
    // uint8_t fptags[8];
    __CRETE_ADD_VCPU_REG_PAIR_ARRAY(ret, uint8_t, fptags, 8)
    // FPReg fpregs[8];
    __CRETE_ADD_VCPU_REG_PAIR_ARRAY(ret, FPReg, fpregs, 8)
    /* KVM-only so far */
    // uint16_t fpop;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint16_t, fpop)
    // uint64_t fpip;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, fpip)
    // uint64_t fpdp;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, fpdp)

    /* emulator internal variables */
    // float_status fp_status;
    __CRETE_ADD_VCPU_REG_PAIR(ret, float_status, fp_status)
    // floatx80 ft0;
    __CRETE_ADD_VCPU_REG_PAIR(ret, floatx80, ft0)

    // float_status mmx_status;
    __CRETE_ADD_VCPU_REG_PAIR(ret, float_status, mmx_status)
    // float_status sse_status;
    __CRETE_ADD_VCPU_REG_PAIR(ret, float_status, sse_status)
    // uint32_t mxcsr;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint32_t, mxcsr)
    // XMMReg xmm_regs[CPU_NB_REGS == 8 ? 8 : 32];
    __CRETE_ADD_VCPU_REG_PAIR_ARRAY(ret, XMMReg, xmm_regs, CPU_NB_REGS == 8 ? 8 : 32)
    // XMMReg xmm_t0;
    __CRETE_ADD_VCPU_REG_PAIR(ret, XMMReg, xmm_t0)
    // MMXReg mmx_t0;
    __CRETE_ADD_VCPU_REG_PAIR(ret, MMXReg, mmx_t0)

    // uint64_t opmask_regs[NB_OPMASK_REGS];
    __CRETE_ADD_VCPU_REG_PAIR_ARRAY(ret, uint64_t, opmask_regs, NB_OPMASK_REGS)

    /* sysenter registers */
    // uint32_t sysenter_cs;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint32_t, sysenter_cs)
    // target_ulong sysenter_esp;
    __CRETE_ADD_VCPU_REG_PAIR(ret, target_ulong, sysenter_esp)
    // target_ulong sysenter_eip;
    __CRETE_ADD_VCPU_REG_PAIR(ret, target_ulong, sysenter_eip)
    // uint64_t efer;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, efer)
    // uint64_t star;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, star)

    // uint64_t vm_hsave;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, vm_hsave)

#ifdef TARGET_X86_64
    // target_ulong lstar;
    __CRETE_ADD_VCPU_REG_PAIR(ret, target_ulong, lstar)
    // target_ulong cstar;
    __CRETE_ADD_VCPU_REG_PAIR(ret, target_ulong, cstar)
    // target_ulong fmask;
    __CRETE_ADD_VCPU_REG_PAIR(ret, target_ulong, fmask)
    // target_ulong kernelgsbase;
    __CRETE_ADD_VCPU_REG_PAIR(ret, target_ulong, kernelgsbase)
#endif

    // uint64_t tsc;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, tsc)
    // uint64_t tsc_adjust;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, tsc_adjust)
    // uint64_t tsc_deadline;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, tsc_deadline)

    // uint64_t mcg_status;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, mcg_status)
    // uint64_t msr_ia32_misc_enable;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, msr_ia32_misc_enable)
    // uint64_t msr_ia32_feature_control;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, msr_ia32_feature_control)

    // uint64_t msr_fixed_ctr_ctrl;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, msr_fixed_ctr_ctrl)
    // uint64_t msr_global_ctrl;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, msr_global_ctrl)
    // uint64_t msr_global_status;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, msr_global_status)
    // uint64_t msr_global_ovf_ctrl;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, msr_global_ovf_ctrl)
    // uint64_t msr_fixed_counters[MAX_FIXED_COUNTERS];
    __CRETE_ADD_VCPU_REG_PAIR_ARRAY(ret, uint64_t, msr_fixed_counters, MAX_FIXED_COUNTERS)
    // uint64_t msr_gp_counters[MAX_GP_COUNTERS];
    __CRETE_ADD_VCPU_REG_PAIR_ARRAY(ret, uint64_t, msr_gp_counters, MAX_GP_COUNTERS)
    // uint64_t msr_gp_evtsel[MAX_GP_COUNTERS];
    __CRETE_ADD_VCPU_REG_PAIR_ARRAY(ret, uint64_t, msr_gp_evtsel, MAX_GP_COUNTERS)

    // uint64_t pat;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, pat)
    // uint32_t smbase;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint32_t, smbase)

    /* End of state preserved by INIT (dummy marker).  */
// xxx: not traced
//    struct {} end_init_save;
//    __CRETE_ADD_VCPU_REG_PAIR(ret, struct {}, end_init_save)

    // uint64_t system_time_msr;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, system_time_msr)
    // uint64_t wall_clock_msr;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, wall_clock_msr)
    // uint64_t steal_time_msr;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, steal_time_msr)
    // uint64_t async_pf_en_msr;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, async_pf_en_msr)
    // uint64_t pv_eoi_en_msr;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, pv_eoi_en_msr)

    // uint64_t msr_hv_hypercall;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, msr_hv_hypercall)
    // uint64_t msr_hv_guest_os_id;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, msr_hv_guest_os_id)
    // uint64_t msr_hv_vapic;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, msr_hv_vapic)
    // uint64_t msr_hv_tsc;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, msr_hv_tsc)

    /* exception/interrupt handling */

// xxx: not traced
// int error_code;
//    __CRETE_ADD_VCPU_REG_PAIR(ret, int, error_code)

    // int exception_is_int;
    __CRETE_ADD_VCPU_REG_PAIR(ret, int, exception_is_int)
    // target_ulong exception_next_eip;
    __CRETE_ADD_VCPU_REG_PAIR(ret, target_ulong, exception_next_eip)
    // target_ulong dr[8];
    __CRETE_ADD_VCPU_REG_PAIR_ARRAY(ret, target_ulong, dr, 8)

//xxx: not traced
//    union {
//        struct CPUBreakpoint *cpu_breakpoint[4];
//        struct CPUWatchpoint *cpu_watchpoint[4];
//    };

    // int old_exception;
    __CRETE_ADD_VCPU_REG_PAIR(ret, int, old_exception)
    // uint64_t vm_vmcb;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, vm_vmcb)
    // uint64_t tsc_offset;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, tsc_offset)
    // uint64_t intercept;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint64_t, intercept)
    // uint16_t intercept_cr_read;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint16_t, intercept_cr_read)
    // uint16_t intercept_cr_write;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint16_t, intercept_cr_write)
    // uint16_t intercept_dr_read;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint16_t, intercept_dr_read)
    // uint16_t intercept_dr_write;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint16_t, intercept_dr_write)
    // uint32_t intercept_exceptions;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint32_t, intercept_exceptions)
    // uint8_t v_tpr;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint8_t, v_tpr)

    /* KVM states, automatically cleared on reset */
    // uint8_t nmi_injected;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint8_t, nmi_injected)
    // uint8_t nmi_pending;
    __CRETE_ADD_VCPU_REG_PAIR(ret, uint8_t, nmi_pending)

// TODO: xxx not traced
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


const Analyzer::cpuRegsTable_ty Analyzer::guest_vcpu_regs_black_list_ =
        Analyzer::init_guest_vcpu_regs_black_list();
const Analyzer::cpuRegsTable_ty Analyzer::guest_vcpu_regs_table_ =
        Analyzer::init_guest_vcpu_regs_table();

static Analyzer analyzer;
static bool is_block_branching = false;

inline
bool is_current_block_symbolic()
{
    return analyzer.is_block_symbolic();
}

inline
void mark_block_branching()
{
    is_block_branching = true;
}

} // namespace tci
} // namespace crete


using namespace crete::tci;

// Generate assumption: within one tci operation, if there is any tainted value being read,
// all the coming writes (to vcpu, guest memory, tci_regs) will be tainted.
static
bool crete_read_was_symbolic = false;

bool crete_tci_is_current_block_symbolic()
{
    return is_current_block_symbolic();
}

void crete_tci_next_tci_instr(void)
{
    crete_read_was_symbolic = false;
}

// Being used by qemu_st_i32/i64, as symbolic address with
// concrete value should not propagate taints
bool crete_tci_get_crete_read_was_symbolic()
{
    return crete_read_was_symbolic;
}
void crete_tci_set_crete_read_was_symbolic(bool input)
{
    crete_read_was_symbolic = input;
}

void crete_tci_read_reg(uint64_t index, uint64_t data)
{
    if(analyzer.is_tcg_reg_symbolic(index, data))
    {
        crete_read_was_symbolic = true;

        //Assumption: the tci_reg[] is not shared across TBs
        if(!analyzer.is_block_symbolic()) {
            fprintf(stderr, "[CRETE ERROR] crete_tci_read_reg(): tci_reg_[%lu] is "
                    "symbolic without any previous assignments, which indicates this "
                    "tci_reg is shared between different TBs\n", index);

            assert(0);
        }
    }
}

void crete_tci_write_reg(uint64_t index, uint64_t data)
{
    if(crete_read_was_symbolic)
    {
        analyzer.make_tcg_reg_symbolic(index, data);
    }
    else
    {
        analyzer.make_tcg_reg_concrete(index, data);
    }
}

/*
// For helper function calls:
// 1. Under the assumption all the inputs are not pointers, we mark the return value as symbolic
//    if there is a symbolic input.
// 2. If the first input is a pointer to CPUState, we further check whether other inputs point
//    to symbolic values or not.
//    If they do, we mark as symbolic the memories (16 Bytes) pointed
//    by the input pointers. If the do not, we mark them as concrete.
//    Note: we only check input pointers within the range of CPUState.
void crete_tci_call_64(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4)
{
    // If there is symbolic input or the first input is not a pointer to cpustate, just return
    if(crete_read_was_symbolic ||
            (arg0 != guest_vcpu_addr_))
        return;

    std::vector<uint64_t> args;
    args.push_back(arg0);
    args.push_back(arg1);
    args.push_back(arg2);
    args.push_back(arg3);
    args.push_back(arg4);

    // 2. If the first input is a pointer to CPUState, we further check whether other inputs
    //    point to symbolic values or not.
    //    Note: we only check input pointers within the range of CPUState.
    std::vector<bool> args_within_vcpu;
    args_within_vcpu.push_back(false); //args should always be cpuState pointer, and we don't check this one
    bool pointed_symbolic = false;
    for(int i = 1; i < 5; ++i)
    {
        bool within_vcpu = analyzer.is_within_vcpu(args[i], 16);
        args_within_vcpu.push_back(within_vcpu);

        if(within_vcpu && analyzer.is_host_mem_symbolic(guest_vcpu_addr_,
                        (args[i] - guest_vcpu_addr_), 16))
            pointed_symbolic = true;
    }
    assert(args.size() == 5 && args_within_vcpu.size() == 5);

    if(pointed_symbolic) {
        crete_read_was_symbolic = true;
        analyzer.mark_block_symbolic();

        // Mark symbolic
        for(int i = 1; i < 5; ++i){
            if(args_within_vcpu[i]) {
                analyzer.make_host_mem_symbolic(guest_vcpu_addr_,
                        (args[i] - guest_vcpu_addr_), 16);

//                char buf[50];
//                crete_print_x86_cpu_regs(args[i] - guest_vcpu_addr_, 16, buf);
//                fprintf(stderr, "Mark symbolic arg[%d] = %p, %s\n", i, (void *)args[i], buf);
            }
        }
    } else {
        // Mark Concrete
        for(int i = 1; i < 5; ++i){
            if(args_within_vcpu[i]) {
                analyzer.make_host_mem_concrete(guest_vcpu_addr_,
                        (args[i] - guest_vcpu_addr_), 16);
                char buf[50];
                crete_print_x86_cpu_regs(args[i] - guest_vcpu_addr_, 16, buf);
//                fprintf(stderr, "Mark concrete arg[%d] = %p, %s\n", i, (void *)args[i], buf);
            }
        }
    }
}
*/
/* Notes:
 * t1 and offset are always constant values, but they represents a host memory address.
 */
void crete_tci_ld8u_i32(uint64_t t0, uint64_t t1, uint64_t offset)
{
    if(analyzer.is_host_mem_symbolic(t1, offset, 1))
        crete_read_was_symbolic = true;
}

void crete_tci_ld_i32(uint64_t t0, uint64_t t1, uint64_t offset)
{
    if(analyzer.is_host_mem_symbolic(t1, offset, 4))
        crete_read_was_symbolic = true;
}

void crete_tci_st8_i32(uint64_t t1, uint64_t offset)
{
    if(crete_read_was_symbolic)
    {
        analyzer.make_host_mem_symbolic(t1, offset, 1);
    }
    else
    {
        analyzer.make_host_mem_concrete(t1, offset, 1);
    }
}

void crete_tci_st16_i32(uint64_t t1, uint64_t offset)
{
    if(crete_read_was_symbolic)
    {
        analyzer.make_host_mem_symbolic(t1, offset, 2);
    }
    else
    {
        analyzer.make_host_mem_concrete(t1, offset, 2);
    }
}

void crete_tci_st_i32(uint64_t t1, uint64_t offset)
{
    if(crete_read_was_symbolic)
    {
        analyzer.make_host_mem_symbolic(t1, offset, 4);
    }
    else
    {
        analyzer.make_host_mem_concrete(t1, offset, 4);
    }
}

void crete_tci_ld8u_i64(uint64_t t0, uint64_t t1, uint64_t offset)
{
    crete_tci_ld8u_i32(t0, t1, offset);
}

void crete_tci_ld32u_i64(uint64_t t0, uint64_t t1, uint64_t offset)
{
    crete_tci_ld_i32(t0, t1, offset);
}

void crete_tci_ld32s_i64(uint64_t t0, uint64_t t1, uint64_t offset)
{
    crete_tci_ld_i32(t0, t1, offset);
}

void crete_tci_ld_i64(uint64_t t0, uint64_t t1, uint64_t offset)
{
    if(analyzer.is_host_mem_symbolic(t1, offset, 8))
        crete_read_was_symbolic = true;
}

void crete_tci_st8_i64(uint64_t t1, uint64_t offset)
{
    crete_tci_st8_i32(t1, offset);
}

void crete_tci_st16_i64(uint64_t t1, uint64_t offset)
{
    crete_tci_st16_i32(t1, offset);
}

void crete_tci_st32_i64(uint64_t t1, uint64_t offset)
{
    crete_tci_st_i32(t1, offset);
}

void crete_tci_st_i64(uint64_t t1, uint64_t offset)
{
    if(crete_read_was_symbolic)
    {
        analyzer.make_host_mem_symbolic(t1, offset, 8);
    }
    else
    {
        analyzer.make_host_mem_concrete(t1, offset, 8);
    }
}

// crete_read_was_symbolic must be over-written, so that symbolic
// address will not cause over-tainting with concrete value
void crete_tci_qemu_ld8u(uint64_t t0, uint64_t addr, uint64_t data)
{
    crete_read_was_symbolic = analyzer.is_guest_mem_symbolic(addr, 1, data);
}

void crete_tci_qemu_ld8s(uint64_t t0, uint64_t addr, uint64_t data)
{
    crete_tci_qemu_ld8u(t0, addr, data);
}

void crete_tci_qemu_ld16u(uint64_t t0, uint64_t addr, uint64_t data)
{
    crete_read_was_symbolic =
            analyzer.is_guest_mem_symbolic(addr, 2, data);
}

void crete_tci_qemu_ld16s(uint64_t t0, uint64_t addr, uint64_t data)
{
    crete_tci_qemu_ld16u(t0, addr, data);
}

void crete_tci_qemu_ld32u(uint64_t t0, uint64_t addr, uint64_t data)
{
    crete_read_was_symbolic =
            analyzer.is_guest_mem_symbolic(addr, 4, data);
}

void crete_tci_qemu_ld32s(uint64_t t0, uint64_t addr, uint64_t data)
{
    crete_tci_qemu_ld32u(t0, addr, data);
}

void crete_tci_qemu_ld32(uint64_t t0, uint64_t addr, uint64_t data)
{
    crete_tci_qemu_ld32u(t0, addr, data);
}

void crete_tci_qemu_ld64(uint64_t t0, uint64_t addr, uint64_t data)
{
    crete_read_was_symbolic =
            analyzer.is_guest_mem_symbolic(addr, 8, data);
}

//TODO: xxx double check ld64_32
//void crete_tci_qemu_ld64_32(uint64_t t0, uint64_t t1, uint64_t addr)
//{
//    if(analyzer.is_guest_mem_symbolic(addr, 8, data))
//        crete_read_was_symbolic = true;
//}

void crete_tci_qemu_st8(uint64_t addr, uint64_t data)
{
    if(crete_read_was_symbolic)
    {
        analyzer.make_guest_mem_symbolic(addr, 1, data);
    }
    else
    {
        analyzer.make_guest_mem_concrete(addr, 1, data);
    }
}

void crete_tci_qemu_st16(uint64_t addr, uint64_t data)
{
    if(crete_read_was_symbolic)
    {
        analyzer.make_guest_mem_symbolic(addr, 2, data);
    }
    else
    {
        analyzer.make_guest_mem_concrete(addr, 2, data);
    }
}

void crete_tci_qemu_st32(uint64_t addr, uint64_t data)
{
    if(crete_read_was_symbolic)
    {
        analyzer.make_guest_mem_symbolic(addr, 4, data);
    }
    else
    {
        analyzer.make_guest_mem_concrete(addr, 4, data);
    }
}

void crete_tci_qemu_st64(uint64_t addr, uint64_t data)
{
    if(crete_read_was_symbolic)
    {
        analyzer.make_guest_mem_symbolic(addr, 8, data);
    }
    else
    {
        analyzer.make_guest_mem_concrete(addr, 8, data);
    }
}

void crete_tci_crete_make_concolic(uint64_t addr, uint64_t size,
        const std::vector<uint8_t>& data)
{
    for(uint64_t i = 0; i < size; ++i)
        analyzer.make_guest_mem_symbolic(addr+i, 1, data[i]);

    crete_tci_next_block(); // FIXME: xxx to force not dumping the current tb
    crete_tci_next_block();
}

void crete_tci_next_block()
{
    analyzer.terminate_block();
    is_block_branching = false;
}

//FIXME: xxx Seems to be unless?
//      Being use to force dump the TB calling this function
void crete_tci_mark_block_symbolic()
{
    analyzer.mark_block_symbolic();
}

bool crete_tci_is_block_branching()
{
    return is_block_branching;
}

void crete_tci_brcond()
{
    mark_block_branching();
}


void tci_analyzer_print()
{
    analyzer.dbg_print();
}

void crete_tci_next_iteration()
{
    analyzer = Analyzer();
}

bool crete_tci_is_previous_block_symbolic()
{
    return analyzer.is_previous_block_symbolic();
}


void crete_init_analyzer(uint64_t guest_vcpu_addr, uint64_t tcg_sp_value)
{
    analyzer.init(guest_vcpu_addr, tcg_sp_value);
}
