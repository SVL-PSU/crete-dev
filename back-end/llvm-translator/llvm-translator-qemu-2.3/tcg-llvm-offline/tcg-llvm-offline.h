#ifndef TCG_LLVM_OFFLINE_H
#define TCG_LLVM_OFFLINE_H

#ifdef __cplusplus
extern "C" {
#endif
#include "stdint.h"

struct TCGLLVMOfflineContext;
extern const struct TCGLLVMOfflineContext *g_tcg_llvm_offline_ctx;

extern uint64_t helpers_size;
extern uint64_t opc_defs_size;

const char * get_helper_name(uint64_t func_addr);

#ifdef __cplusplus
}
#endif


#ifdef __cplusplus

#include <vector>
#include <stdint.h>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>

using namespace std;

struct TCGContext;
struct TCGTemp;
class TCGLLVMOfflineContext
{
private:
    // Required information from QEMU for the offline translation
    vector<uint64_t> m_tlo_tb_pc;

    vector<TCGContext> m_tcg_ctx;
    vector<vector<TCGTemp> > m_tcg_temps;
    map<uint64_t, string> m_helper_names;

    vector<uint64_t> m_tlo_tb_inst_count;

    // Execution sequence of cpatured TB: <pc, unique-tb-number>
    vector<pair<uint64_t, uint64_t> > m_tbExecSequ;
    uint64_t m_cpuState_size;

public:
    TCGLLVMOfflineContext() {};
    ~TCGLLVMOfflineContext() {};

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & m_tlo_tb_pc;

        ar & m_tcg_ctx;
        ar & m_tcg_temps;
        ar & m_helper_names;

        ar & m_tlo_tb_inst_count;

        ar & m_tbExecSequ;
        ar & m_cpuState_size;
    }

#if !defined(TCG_LLVM_OFFLINE)
    void dump_tlo_tb_pc(const uint64_t pc);

    void dump_tcg_ctx(const TCGContext& tcg_ctx);
    void dump_tcg_temp(const vector<TCGTemp>& tcg_temp);
    void dump_tcg_helper_name(const TCGContext &tcg_ctx);

    void dump_tlo_tb_inst_count(const uint64_t inst_count);

    void dump_tbExecSequ(uint64_t pc, uint64_t unique_tb_num);
    void dump_cpuState_size(uint64_t cpuState_size);
#endif

    uint64_t get_tlo_tb_pc(const uint64_t tb_index) const;

    const TCGContext& get_tcg_ctx(const uint64_t tb_index) const;
    const vector<TCGTemp>& get_tcg_temp(const uint64_t tb_index) const;
    const map<uint64_t, string> get_helper_names() const;

    uint64_t get_tlo_tb_inst_count(const uint64_t tb_index) const;

    vector<pair<uint64_t, uint64_t> > get_tbExecSequ() const;
    uint64_t get_cpuState_size() const;

    void print_info();
    void dump_verify();
    uint64_t get_size();
};

#endif // #ifdef __cplusplus

#endif //#ifndef TCG_LLVM_OFFLINE_H
