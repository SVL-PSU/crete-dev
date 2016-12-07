#ifndef RUNTIME_DUMP_H
#define RUNTIME_DUMP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include "config.h"

struct TCGContext;
struct TranslationBlock;
struct TCGOpDef;

struct RuntimeEnv;
struct CreteFlags;

/*****************************/
/* Globals for QEMU c code */
extern struct RuntimeEnv *runtime_env;
extern struct CreteFlags *g_crete_flags;

extern struct TranslationBlock *rt_dump_tb;
extern uint64_t rt_dump_tb_count;
extern uint64_t nb_captured_llvm_tb;

extern int flag_rt_dump_enable;
extern int flag_interested_tb;
extern int flag_interested_tb_prev;

// Enabled/Disabled on capture_begin/end. Can be disabled on command (e.g., crete_debug_capture()).
extern int crete_flag_capture_enabled;

extern int is_begin_capture;
extern int is_target_pid;
extern int is_user_code;

extern uint64_t g_crete_target_pid;
extern int g_custom_inst_emit;

#if defined(TARGET_X86_64)
    #define USER_CODE_RANGE 0x00007FFFFFFFFFFF
#elif defined(TARGET_I386)
    #define USER_CODE_RANGE 0xC0000000 // TODO: Should technically be 0xBFFFFFFF
#else
    #error CRETE: Only I386 and x64 supported!
#endif // defined(TARGET_X86_64) || defined(TARGET_I386)

/*****************************/
/* Functions for QEMU c code */

void crete_runtime_dump_initialize(void);
void crete_runtime_dump_close(void);

/* Setup tracing before the virtual cpu executes a translation block*/
void crete_pre_cpu_tb_exec(void *qemuCpuState, TranslationBlock *tb);
/* Finish-up tracing after the virtual cpu executes a translation block
 * Return: whether the current tb is interested after post_runtime_dump
 * */
int  crete_post_cpu_tb_exec(void *qemuCpuState, TranslationBlock *tb,
		uint64_t next_tb, uint64_t crete_interrupted_pc);
void dump_memo_sync_table_entry(struct RuntimeEnv *rt, uint64_t addr,
		uint32_t size, uint64_t value);

void add_qemu_interrupt_state(struct RuntimeEnv *rt,
		int intno, int is_int, int error_code, int next_eip_addend);

void crete_set_capture_enabled(struct CreteFlags *cf, int capture_enabled);
int  crete_flags_is_true(struct CreteFlags *cf);

#ifdef __cplusplus
}
#endif

/*****************************/
/* C++ code */
#ifdef __cplusplus

#include <string>
#include <stdlib.h>
#include <vector>
#include <set>
#include <map>
#include <stack>
#include <sstream>
#include <iostream>
#include <fstream>

#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/unordered_map.hpp>

/***********************************/
/* External interface for C++ code */
#include "tcg-llvm-offline/tcg-llvm-offline.h"

using namespace std;

enum MemoMergePoint_ty{
    NormalTb = 0,
    BackToInterestTb = 1,
    OutofInterestTb = 2,
    //special case for the tb which is not only the first tb from disinterested tb
    //to interested tb, but also the last tb from interest tb to disinterested tb
    OutAndBackTb = 3
};

struct CreteMemoInfo
{
    uint64_t m_addr;
    uint32_t m_size;
    string m_name;
    vector<uint8_t> m_data;
    bool m_addr_valid;

    CreteMemoInfo(uint64_t addr, uint32_t size, vector<uint8_t> data)
    :m_addr(addr), m_size(size), m_name("concreteMemo"),
     m_data(data), m_addr_valid(true){}

    CreteMemoInfo(uint32_t size, string name, vector<uint8_t> data)
    :m_addr(0), m_size(size), m_name(name),
     m_data(data), m_addr_valid(false) {}

    // For serialization
    CreteMemoInfo()
    :m_addr(0), m_size(0), m_name(string()),
     m_data(vector<uint8_t>()), m_addr_valid(false) {}

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & m_addr;
        ar & m_size;
        ar & m_name;
        ar & m_data;
        ar & m_addr_valid;
    }
};

struct CPUStateElement{
    uint64_t m_offset;
    uint64_t m_size;
    string m_name;
    vector<uint8_t> m_data;

    CPUStateElement(uint64_t offset, uint64_t size, string name, vector<uint8_t> data)
    :m_offset(offset), m_size(size), m_name(name), m_data(data) {}

    // For serialization
    CPUStateElement()
    :m_offset(0), m_size(0), m_name(string()),
     m_data(vector<uint8_t>()) {}

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & m_offset;
        ar & m_size;
        ar & m_name;
        ar & m_data;
    }
};

struct QemuInterruptInfo {
    int m_intno;
    int m_is_int;
    int m_error_code;
    int m_next_eip_addend;

    QemuInterruptInfo(int intno, int is_int, int error_code, int next_eip_addend)
    :m_intno(intno), m_is_int(is_int),
     m_error_code(error_code), m_next_eip_addend(next_eip_addend) {}

    // For serialization
    QemuInterruptInfo()
        :m_intno(0), m_is_int(0), m_error_code(0), m_next_eip_addend(0) {}

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & m_intno;
        ar & m_is_int;
        ar & m_error_code;
        ar & m_next_eip_addend;
    }
};

// <address, value>
typedef boost::unordered_map<uint64_t, uint8_t> memoSyncTable_ty;
typedef vector<memoSyncTable_ty> memoSyncTables_ty;

typedef map<uint64_t, CreteMemoInfo> debug_memoSyncTable_ty;
typedef vector<debug_memoSyncTable_ty> debug_memoSyncTables_ty;

// bool: valid table or not
// vector<>: contents
typedef pair<bool, vector<CPUStateElement> > cpuStateSyncTable_ty;

typedef pair<QemuInterruptInfo, bool> interruptState_ty;

//<name, concolic_memo>
typedef map<string, CreteMemoInfo> creteConcolics_ty;

class RuntimeEnv
{
private:
	// Instruction sequence and its translation context
    TCGLLVMOfflineContext m_tcg_llvm_offline_ctx;

    // Initial CPU state
    vector<uint8_t> m_initial_CpuState;
    // CpuState Side-effects
    vector<cpuStateSyncTable_ty> m_cpuStateSyncTables;
    // Two CPU States for tracing the side effects on CPUState
    // A CPUState right after  a set of consecutive interested TBs,
    // which will be compared with a CPUState right before a set of
    // consecutive interested TBs to compute side effects on CPUState
    pair<bool, void *> m_cpuState_post_insterest;
    // The CPUState before the execution of potential interested TBs
    pair<bool, void *> m_cpuState_pre_interest;
    // The CPUState after each interested TB being executed for cross checking on klee side
    vector<cpuStateSyncTable_ty> m_debug_cpuStateSyncTables;

    memoSyncTables_ty m_memoSyncTables;
    memoSyncTable_ty m_currentMemoSyncTable;
    uint64_t m_mergePoint_memoSyncTables;

    // Memory state, being captured on-the-fly by monitoring memory operations of interested TBs
    // Each entry stores all load memory operations for each unique addr for each interested TB
    // Each entry is a memoSyncTable
    debug_memoSyncTables_ty m_debug_memoSyncTables;
    vector<MemoMergePoint_ty> m_debug_memoMergePoints;

    // Streaming tracing
    bool m_streamed;        // Flag to indicate whether the trace have been streamed or not
    bool m_pending_stream;  // Flag to indicate whether this is a pending stream
    uint64_t m_streamed_tb_count;
    uint64_t m_streamed_index;

    // crete miscs:
    // <name, concolic CreteMemoInfo>
    creteConcolics_ty m_concolics;
    vector<string> m_make_concolic_order;

    string m_outputDirectory;

    // For constructing off-line execution graph/tree
    // (contains extra one non-symbolic-tb after each symbolic-tb)
    vector<uint64_t> m_tbGraphExecSequ;

    // For debugging
    vector<uint64_t> m_tbExecSequPC;
    // The interrupts and their state information collected during the execution of the program
    // (in raise_interrupt()), and hence will only contains interrupts invoked
    // by the program (the interrupt raised outsided the program will not be captured, such as
    // page fault when loading code, hardware interrupt from keyboard, timer, etc)
    // TODO: to-be-streamed in the future
    vector< interruptState_ty > m_interruptStates;
    map<uint64_t, string> m_debug_helper_names;

    // name, offset, size
    map<string, pair<uint64_t, uint64_t> > m_debug_cpuState_offsets;

    //
    void *m_dbg_cpuState_post_interest;

public:
	RuntimeEnv();
	~RuntimeEnv();

    // Instruction sequence in terms of TB sequences
	void addTBExecSequ(uint64_t index_captured_llvm_tb, uint64_t tb_pc);
    //x86-llvm context for offline traslation
	void dump_tloCtx(void * cpuState, TranslationBlock *tb,
	        uint64_t crete_interrupted_pc);

	// CPU state and its side-effect
    void addInitialCpuState();

    void addcpuStateSyncTable();
    void addEmptyCPUStateSyncTable();
    void setCPUStatePostInterest(const void *src);
    void setFlagCPUStatePostInterest();
    void setCPUStatePreInterest(const void *src);
    void resetCPUStatePreInterest();

    void addDebugCpuStateSyncTable(void *qemuCpuState);
    void printDebugCpuStateSyncTable(const string name) const;

    // Memory monitor
    void addCurrentMemoSyncTableEntry(uint64_t addr, uint32_t size, uint64_t value);
    void addCurrentMemoSyncTable();
    void mergeCurrentMemoSyncTable();
    void clearCurrentMemoSyncTable();

    // Old MM
    void addMemoSyncTable();
    void addMemoSyncTableEntry(uint64_t addr, uint32_t size, uint64_t value);
    void addMemoMergePoint(MemoMergePoint_ty type_MMP);

    // Stream tracing
    void stream_writeRtEnvToFile(uint64_t tb_count);
    void set_pending_stream();

    //Misc
    void handlecreteMakeConcolic(string name, uint64_t guest_addr, uint64_t size);

    void writeRtEnvToFile();
    void verifyDumpData() const;
    void initOutputDirectory(const string& outputDirectory);

    void reverseTBDump(void *qemuCpuState);

    static int access_guest_memory(const void *env_cpuState, uint64_t addr,
            uint8_t *buf, int len, int is_write);

    // Offline execution graph
    void addTBGraphExecSequ(uint64_t tb_pc);

    // for debugging
    void addQemuInterruptState(QemuInterruptInfo interrup_info);
    void addEmptyQemuInterruptState();
    void printInfo();

    string get_tcoHelper_name(uint64_t) const;

    void init_debug_cpuState_offsets(const vector<pair<string, pair<uint64_t, uint64_t> > >& offsets);

    void set_dbgCPUStatePostInterest(const void *cpuState);
    void check_dbgCPUStatePostInterest(const void *cpuState);

private:
    void init_concolics();

    void dump_tloTbPc(const uint64_t pc);
    void dump_tloTcgCtx(const TCGContext& tcg_ctx);
    void dump_tloHelpers(const TCGContext &tcg_ctx);
    void dump_tcg_temp(const vector<TCGTemp>& tcg_temp);
    void dump_tloTbInstCount(const uint64_t inst_count);

    void writeTcgLlvmCtx();

    string getOutputFilename(const string &fileName) const;

    void writeConcolics();

    // Memory Monitoring
    void writeMemoSyncTables();
    // Old MM
    void debug_mergeMemoSyncTables();
    void debug_writeMemoSyncTables();
    vector<uint64_t> overlapsMemoSyncEntry(uint64_t addr,
            uint32_t size, debug_memoSyncTable_ty target_memoSyncTable);
    void addMemoSyncTableEntryInternal(uint64_t addr, uint32_t size, vector<uint8_t> v_value,
             debug_memoSyncTable_ty& target_memoSyncTable);
    void verifyMemoSyncTable(const debug_memoSyncTable_ty& target_memoSyncTable);
    void debugMergeMemoSync();
    void print_memoSyncTables();

    void writeInitialCpuState();
    void checkEmptyCPUStateSyncTables();
    void writeCPUStateSyncTables();
    void writeDebugCPUStateSyncTables();
    void writeDebugCpuStateOffsets();

    void writeInterruptStates() const;

    void writeTBGraphExecSequ();
};

class CreteFlags{
public:
	const void *m_cpuState;
	const TranslationBlock *m_tb;

private:
	uint64_t m_target_pid;  // g_crete_target_pid
	bool m_capture_started; // g_custom_inst_emit

	bool m_capture_enabled; // crete_flag_capture_enabled

	// indicate that whether the current instance of CreteFlag is valid
	bool m_valid;

public:
	CreteFlags();
	~CreteFlags();

	void set(uint64_t target_pid);
	void reset();

	void set_capture_enabled(bool capture_enabled);

	bool is_true() const;
	bool is_true(void* cpuState, TranslationBlock *tb) const;

	void check(bool valid) const;
};
#endif  /* __cplusplus end*/

#endif  /* RUNTIME_DUMP_H end */
