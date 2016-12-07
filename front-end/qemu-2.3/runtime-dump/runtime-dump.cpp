#include "runtime-dump.h"
#include "tci_analyzer.h"
#include "crete-debug.h"

extern "C" {
#include "config.h"
#include "cpu.h"
}
#include "tcg.h"

#include <boost/archive/binary_oarchive.hpp>

#include <crete/test_case.h>
#include <crete/stacktrace.h>

#include <stdexcept>
#include <boost/filesystem/operations.hpp>

#include "custom-instructions.h"

namespace fs = boost::filesystem;
using namespace std;

RuntimeEnv *runtime_env = 0;
CreteFlags *g_crete_flags = NULL;

TranslationBlock *rt_dump_tb = 0;

/* flags for code selection */
int is_begin_capture = 0;
int is_target_pid = 0;
int is_user_code = 0;

/* TODO: xxx they are fairly messy, b/c
 * flags managed in custom-instruction
 * */
uint64_t g_crete_target_pid = 0;
// Flag for indicating whether the custom_instruction "capture_begin" emitted
// Will be reset in crete_tracing_reset();
int g_custom_inst_emit = 0;
int crete_flag_capture_enabled = 0;

/* flag for runtime tracing: */
/* 0 = disable tracing, 1 = enable tracing */
int	flag_rt_dump_enable = 0;
/* flag to indicate whether the current tb is of interest*/
int flag_interested_tb = 0;
/* flag to indicate whether the previous tb is of interest*/
int flag_interested_tb_prev = 0;

/* count how many tb that is of interest has been executed including the current one*/
uint64_t rt_dump_tb_count = 0;
uint64_t nb_captured_llvm_tb = 0;

/* global variable of CPUState, which will be used in custom instruction handler.*/
CPUArchState *g_cpuState_bct = 0;

#define CPU_OFFSET(field) offsetof(CPUArchState, field)

static const uint32_t CRETE_TRACING_WINDOW_SIZE = 10000;

/***********************************/
/* External interface for C++ code */
RuntimeEnv::RuntimeEnv()
: m_streamed(false), m_pending_stream(false),
  m_streamed_tb_count(0), m_streamed_index(0)
{
    m_cpuState_post_insterest.first = false;
    m_cpuState_post_insterest.second = new uint8_t [sizeof(CPUArchState)];

    m_cpuState_pre_interest.first = false;
    m_cpuState_pre_interest.second = new uint8_t [sizeof(CPUArchState)];

    m_tcg_llvm_offline_ctx.dump_cpuState_size(sizeof(CPUArchState));
    m_initial_CpuState.reserve(sizeof(CPUArchState));

    CRETE_DBG_INT(m_dbg_cpuState_post_interest = new uint8_t [sizeof(CPUArchState)];);
}

RuntimeEnv::~RuntimeEnv()
{
    assert(m_cpuState_post_insterest.second);
    delete [] (uint8_t *)m_cpuState_post_insterest.second;

    assert(m_cpuState_pre_interest.second);
    delete [] (uint8_t *)m_cpuState_pre_interest.second;

    CRETE_DBG_INT(
    assert(m_dbg_cpuState_post_interest);
    delete [] (uint8_t *)m_dbg_cpuState_post_interest;
    );
}

// Dump the context for offline translation from qemu-ir to llvm bitcode
// TODO: xxx Optimize the way how we trace the tcg_ctx:
//      1. Translate from a copy of current TB,
//      2. Only update the relevant flag of current TB if necessary for caching purpose
void RuntimeEnv::dump_tloCtx(void *cpuState, TranslationBlock *tb, uint64_t crete_interrupted_pc)
{
    if(crete_interrupted_pc == 0) {
        assert(tb->tcg_ctx_captured == 0);
        assert(tb->index_captured_llvm_tb == -1);
    } else {
        assert(crete_interrupted_pc > tb->pc);
    }

    // Dump the tcg_ctx for the current tb
    CPUArchState *env = (CPUArchState*) cpuState;
    TCGContext *s = &tcg_ctx;
    assert(s);

    //helper_names, only will be dumped once
    if(nb_captured_llvm_tb == 0)
        dump_tloHelpers(*s);

    tcg_func_start(s);
	gen_intermediate_code_crete(env, tb, crete_interrupted_pc);
//	gen_intermediate_code_pc(env,tb);

    // the number of instructions within this tb
    uint64_t tb_inst_count = tcg_tb_inst_count(s);
	dump_tloTbInstCount(tb_inst_count);

//	cerr << "tb_inst_count = " << dec << tb_inst_count << endl;

    // tb->pc
    dump_tloTbPc((uint64_t)tb->pc);

    // tcg_ctx
    dump_tloTcgCtx(*s);

    //tcg_temp
    vector<TCGTemp> temp_tcg_temp;
    for(uint32_t i = 0; i < (uint32_t)s->nb_temps; ++i)
    	temp_tcg_temp.push_back(s->temps[i]);
    dump_tcg_temp(temp_tcg_temp);

    tb->index_captured_llvm_tb = nb_captured_llvm_tb++;

    //for debug purpsoe, qemu ir
//    dump_IR(s, tb->pc);
}

void RuntimeEnv::addInitialCpuState()
{
    assert(m_initial_CpuState.empty());
    assert(m_cpuState_pre_interest.first);

    m_initial_CpuState.resize(sizeof(CPUArchState));
    memcpy(m_initial_CpuState.data(), m_cpuState_pre_interest.second, sizeof(CPUArchState));
}

static vector<CPUStateElement> x86_cpuState_compuate_side_effect(const CPUArchState *reference,
        const CPUArchState *target);

void RuntimeEnv::addcpuStateSyncTable()
{
    assert(m_cpuState_post_insterest.first == true);
    assert(m_cpuState_pre_interest.first == true);

    const CPUArchState *post_interest = (const CPUArchState *) m_cpuState_post_insterest.second;
    const CPUArchState *pre_insterest = (const CPUArchState *) m_cpuState_pre_interest.second;

    m_cpuStateSyncTables.push_back(make_pair(true,
            x86_cpuState_compuate_side_effect(post_interest, pre_insterest)));

    // Invalid m_cpuState_post_insterest, after CPUState side-effect is computed
    m_cpuState_post_insterest.first = false;
}

void RuntimeEnv::addEmptyCPUStateSyncTable()
{
    m_cpuStateSyncTables.push_back(make_pair(false, vector<CPUStateElement>()));
}

vector<CPUStateElement> x86_cpuState_dump(const CPUArchState *target);

void RuntimeEnv::addDebugCpuStateSyncTable(void *qemuCpuState)
{
    assert(qemuCpuState);

#if defined(CRETE_CROSS_CHECK)
    m_debug_cpuStateSyncTables.push_back(make_pair(true,
            x86_cpuState_dump((const CPUArchState *)qemuCpuState)));
#else
    m_debug_cpuStateSyncTables.push_back(make_pair(false, vector<CPUStateElement>()));
#endif
}

void RuntimeEnv::printDebugCpuStateSyncTable(const string name) const
{
    vector<CPUStateElement> last = m_debug_cpuStateSyncTables.back().second;

    cerr << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n";
    cerr << "m_debug_cpuStateSyncTables.size(): " << dec << m_debug_cpuStateSyncTables.size() << endl;
    for(vector<CPUStateElement>::const_iterator it = last.begin();
            it != last.end(); ++it) {
        if(it->m_name.find(name) != string::npos) {
            fprintf(stderr, "%s(%lu, %lu): [",
                    it->m_name.c_str(),
                    it->m_offset, it->m_size);
            for(vector<uint8_t>::const_iterator cit = it->m_data.begin();
                    cit != it->m_data.end(); ++cit) {
                cerr << hex << "0x" << (uint64_t)(*cit) << " ";
            }
            cerr << "]\n";
        }
    }
    cerr << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n";
}

void RuntimeEnv::addCurrentMemoSyncTableEntry(uint64_t addr, uint32_t size, uint64_t value)
{
    assert(size <= 8);
    uint8_t byte_value;
    for(uint64_t i = 0; i < size ; ++i){
        byte_value = (value >> i*8) & 0xff;
        m_currentMemoSyncTable.insert(make_pair(addr + i, byte_value));
    }
}

// Add m_currentMemoSyncTable to m_memoSyncTables
void RuntimeEnv::addCurrentMemoSyncTable()
{
    m_memoSyncTables.push_back(m_currentMemoSyncTable);
    m_mergePoint_memoSyncTables = m_memoSyncTables.size() - 1;
}

void RuntimeEnv::mergeCurrentMemoSyncTable()
{
    assert(m_mergePoint_memoSyncTables < m_memoSyncTables.size());

    m_memoSyncTables.push_back(memoSyncTable_ty());
    m_memoSyncTables[m_mergePoint_memoSyncTables].insert(m_currentMemoSyncTable.begin(),
            m_currentMemoSyncTable.end());
}

void RuntimeEnv::clearCurrentMemoSyncTable()
{
    m_currentMemoSyncTable.clear();
}

#if defined(CRETE_DBG_MEM_MONI)
// add an empty MemoSyncTable to m_debug_memoSyncTables which will store
// all the load memory operations for an interested TB
void RuntimeEnv::addMemoSyncTable()
{
#if defined(CRETE_DEBUG)
    if(!m_debug_memoSyncTables.empty()){
        cerr << "verifying MemoSyncTable, size = " << dec << m_debug_memoSyncTables.size() << endl;
        verifyMemoSyncTable(m_debug_memoSyncTables.back());
    }
#endif

    m_debug_memoSyncTables.push_back(debug_memoSyncTable_ty());

#if defined(CRETE_DEBUG)
    cerr << "[Ld Memo Monitor]addMemoSyncTable is invoked. Number of Memo Sync Table is "
            << m_debug_memoSyncTables.size() << endl;
#endif // defined(CRETE_DEBUG)
}

// add an memoSyncTableEntry (which is actually a memory operation)
// to the memoSyncTable at the end of m_debug_memoSyncTables
void RuntimeEnv::addMemoSyncTableEntry(uint64_t addr, uint32_t size, uint64_t value)
{
#if defined(CRETE_DEBUG)
    cerr << "[Ld Memo Monitor] addMemoSyncTableEntry is invoked.\n";
#endif // defined(CRETE_DEBUG)

    assert(size <= 8 && "[CRETE ERROR] Data size dumped from qemu is more than 8 bytes!\n");
    debug_memoSyncTable_ty &last_memoSyncTable = m_debug_memoSyncTables.back();

    vector<uint8_t> v_value;
    for(uint32_t i = 0; i < size; ++i) {
        uint8_t temp_value = (value >> i*8) & 0xff;
        v_value.push_back(temp_value);
    }

    addMemoSyncTableEntryInternal(addr, size, v_value, last_memoSyncTable);

#if defined(CRETE_DEBUG)
    cerr << "[Ld Memo Monitor] After insert current MemoSyncTable size is " << last_memoSyncTable.size() << endl;
#endif // defined(CRETE_DEBUG)
}

void RuntimeEnv::addMemoMergePoint(MemoMergePoint_ty type_MMP)
{
    if(m_debug_memoMergePoints.empty()) {
        assert(flag_interested_tb == 1);
#if defined(CRETE_DEBUG)
        cerr << "[Ld Memo Monitor] First interested TB's mergePoint type is " << type_MMP << endl;
#endif // defined(CRETE_DEBUG)
    }

    if(type_MMP == NormalTb || type_MMP == BackToInterestTb){
        assert(flag_interested_tb == 1);
        m_debug_memoMergePoints.push_back(type_MMP);

#if defined(CRETE_DEBUG)
    cerr << "[addMemoMergePoint] " << m_debug_memoMergePoints.size()
            << ", merge point type is " << type_MMP << endl;
#endif // defined(CRETE_DEBUG)

        return;
    }

    assert(type_MMP == OutofInterestTb);
    assert(flag_interested_tb == 0);

    // For the situation of OutofInterestTb, we need to update the last element in m_debug_memoMergePoints
    MemoMergePoint_ty &current_tb_MMP = m_debug_memoMergePoints.back();

    if (current_tb_MMP ==  NormalTb)
        current_tb_MMP = OutofInterestTb;
    else if (current_tb_MMP ==  BackToInterestTb)
        current_tb_MMP = OutAndBackTb;
    else
        assert(0 && "[CRETE ERROR] Unexpected type of MemoMergePoint_ty\n");

#if defined(CRETE_DEBUG)
    cerr << "[addMemoMergePoint] size = " << m_debug_memoMergePoints.size()
            << ", merge point type is " << current_tb_MMP << endl;
#endif // defined(CRETE_DEBUG)
}
#endif //#if defined(CRETE_DBG_MEM_MONI)


void RuntimeEnv::addTBExecSequ(uint64_t index_captured_llvm_tb, uint64_t tb_pc)
{
    m_tcg_llvm_offline_ctx.dump_tbExecSequ(tb_pc, index_captured_llvm_tb);

    addTBGraphExecSequ(tb_pc);

    m_tbExecSequPC.push_back(tb_pc);
}

// Set guest address in m_concolics, and write concrete data from m_concolics into qemu guest memory
void RuntimeEnv::handlecreteMakeConcolic(string name, uint64_t guest_addr, uint64_t size)
{
    if(m_make_concolic_order.empty()) {
        init_concolics();
    }

    creteConcolics_ty::iterator it = m_concolics.find(name);
    if(it != m_concolics.end()){
        // concolic variable from xml
        CreteMemoInfo& concolic_memo = it->second;

        assert(concolic_memo.m_size == size);
        assert(concolic_memo.m_name == name);
        assert(!concolic_memo.m_addr_valid);

        concolic_memo.m_addr = guest_addr;
        concolic_memo.m_addr_valid = true;

        if(RuntimeEnv::access_guest_memory(g_cpuState_bct, guest_addr,
                (uint8_t *)concolic_memo.m_data.data(), concolic_memo.m_size, 1) != 0 ) {
            cerr << "[CRETE ERROR] access_guest_memory() failed within handlecreteMakeConcolic()\n";
            cerr << "concolic variable: " << concolic_memo.m_name << endl;
            assert(0);
        }

        m_make_concolic_order.push_back(concolic_memo.m_name);
    } else {
        // concolic variable made by calling crete_make_concolic within the executable under test

        fprintf(stderr, "[CRETE Warning] handlecreteMakeConcolic(): %s[%p %lu] is not from input_arguments.bin\n",
                name.c_str(), (void *)guest_addr, size);

        vector<uint8_t> data;
        data.resize(size);

        if(RuntimeEnv::access_guest_memory(g_cpuState_bct, guest_addr,
                (uint8_t *)data.data(), size, 0) != 0 ) {
            cerr << "[CRETE ERROR] access_guest_memory() failed within handlecreteMakeConcolic()\n";
            cerr << "concolic variable: " << name << endl;
            assert(0);
        }

        CreteMemoInfo concolic_memo;
        concolic_memo.m_addr = guest_addr;
        concolic_memo.m_size = size;
        concolic_memo.m_name = name;
        concolic_memo.m_data = data;
        concolic_memo.m_addr_valid = true;

        m_concolics.insert(make_pair(name, concolic_memo));
        m_make_concolic_order.push_back(concolic_memo.m_name);
    }

    it = m_concolics.find(name);
    assert(it != m_concolics.end());
    crete_tci_crete_make_concolic(it->second.m_addr, it->second.m_size, it->second.m_data);
}

// Generate output files for runtime environment
void RuntimeEnv::writeRtEnvToFile()
{
    try
    {
        if(rt_dump_tb_count == 0) {
            cerr << "[CRETE Warning] writeRtEnvToFil() returned with nothing dumped.\n" << endl;
            return;
        }

        verifyDumpData();

        if(!m_streamed)
        {
            initOutputDirectory("");
            writeInitialCpuState();
            writeDebugCpuStateOffsets();
        }

        // streamed
        writeTcgLlvmCtx();
        writeCPUStateSyncTables();
        writeDebugCPUStateSyncTables();
        writeMemoSyncTables();

        // to-be-streamed
        writeInterruptStates();
#if defined(CRETE_DBG_MEM_MONI)
        debug_writeMemoSyncTables();
#endif

        // need-not-streamed
        writeConcolics();
        writeTBGraphExecSequ();
    }
    catch(std::exception& e)
    {
        std::cerr << "[CRETE Exception] " << e.what() << std::endl;
        print_stacktrace();
    }
}

void RuntimeEnv::stream_writeRtEnvToFile(uint64_t tb_count) {
    if(!m_pending_stream)
        return;

    if(!m_streamed)
    {
        initOutputDirectory("");
        writeInitialCpuState();
        writeDebugCpuStateOffsets();
    }

    writeTcgLlvmCtx();
    writeCPUStateSyncTables();
    writeDebugCPUStateSyncTables();
    writeMemoSyncTables();

    m_streamed = true;
    m_pending_stream = false;
    ++m_streamed_index;
    m_streamed_tb_count = tb_count;
}

void RuntimeEnv::set_pending_stream()
{
    m_pending_stream = true;
}

void RuntimeEnv::printInfo()
{
#if defined(CRETE_DEBUG) || defined(CRETE_DBG_CALL_STACK)
    static unsigned current_dump_id = 0;
    cerr << "========================================\n"
            << "Statistic of run-time environment dump:\n"
            << "Dump ID: " << current_dump_id++ << '\n'
            << "Number of CPUArchStates: " << m_cpuStates.size() << '\n'
            << "Number of m_symbMemos: " << m_symbMemos.size() << '\n'
            << "Number of m_tbExecSequ: " << m_tbExecSequ.size() << '\n'
            << "Number of m_tbDecl: " << m_tbDecl.size() << '\n'
            << "Number of m_interruptStates: " << m_interruptStates.size() << '\n'
            << "m_outputDirectory: " << m_outputDirectory << '\n'
            << "========================================\n";
#endif // defined(CRETE_DEBUG)
}

int RuntimeEnv::access_guest_memory(const void *env_cpuState, uint64_t addr,
        uint8_t *buf, int len, int is_write) {
    CPUArchState *env = (CPUArchState *) env_cpuState;
    CPUState *cs = CPU(ENV_GET_CPU(env));

    int ret = cpu_memory_rw_debug(cs, (target_ulong)addr, buf, len, is_write);

    return ret;
}

void RuntimeEnv::initOutputDirectory(const string& outputDirectory)
{
    if (outputDirectory.empty()) {
        fs::path cwd = fs::current_path();

        for (int i = 0; ; i++) {
            ostringstream dirName;
            dirName << "runtime-dump-" << i;

            fs::path dirPath(cwd);
            dirPath /= "trace";
            dirPath /= dirName.str();

            bool exists = fs::exists(dirPath);

            if(!exists) {
                m_outputDirectory = dirPath.string();
                break;
            }
        }

    } else {
        m_outputDirectory = outputDirectory;
    }

#if defined(CRETE_DEBUG)
    cerr << "Output directory = \" " << m_outputDirectory << "\"\n";
    printf("printf: Output directory = \" %s\".\n", m_outputDirectory.c_str());
#endif // defined(CRETE_DEBUG)

    fs::path outDir(m_outputDirectory);

    if (!fs::create_directories(outDir)) {
#if defined(CRETE_DEBUG)
        cerr << "Could not create output directory " << outDir.string() << '\n';
#endif // defined(CRETE_DEBUG)
        exit(-1);
    }

    if(!fs::exists(outDir)) {
        assert(0);
    }

    fs::path dumpLast("trace");
    dumpLast /= "runtime-dump-last";

    if ((unlink(dumpLast.string().c_str()) < 0) && (errno != ENOENT)) {
        perror("ERROR: Cannot unlink runtime-dump-last");
        exit(1);
    }

    if (symlink(m_outputDirectory.c_str(), dumpLast.string().c_str()) < 0) {
        perror("ERROR: Cannot make symlink runtime-dump-last");
        exit(1);
    }

}

void RuntimeEnv::reverseTBDump(void *qemuCpuState)
{
#if defined(CRETE_DEBUG)
    cerr << "reverseTBDump(): \n"
         << dec << "rt_dump_tb_count = " << rt_dump_tb_count
         << ", m_prolog_regs.size() = " << m_prolog_regs.size()
         << ", m_debug_memoSyncTables.size() = " << m_debug_memoSyncTables.size()
         << ", m_debug_memoMergePoints.size() = " << m_debug_memoMergePoints.size()
         <<", m_tbExecSequ.size() = " << m_tbExecSequ.size()
         << ", m_interruptStates.size() = " << m_interruptStates.size() << endl;
#endif //#if defined(CRETE_DEBUG)

#if defined(CRETE_DBG_MEM_MONI)
    //undo memo tracing
    m_debug_memoSyncTables.pop_back();

    m_debug_memoMergePoints.pop_back();
    assert(flag_interested_tb == 0);
    if(flag_interested_tb_prev == 1 && flag_interested_tb == 0) {
        runtime_env->addMemoMergePoint(OutofInterestTb);
    }
#endif

    verifyDumpData();
}

void RuntimeEnv::addQemuInterruptState(QemuInterruptInfo interrup_info)
{
	m_interruptStates.pop_back();
	m_interruptStates.push_back(make_pair(interrup_info, true));
}

void RuntimeEnv::addEmptyQemuInterruptState()
{
    QemuInterruptInfo empty_intterrup_info(0,0,0,0);
    m_interruptStates.push_back(make_pair(empty_intterrup_info, false));
}

/*
 * To verify the number of dumped TBs in RuntimeEnv is valid*/
void RuntimeEnv::verifyDumpData() const
{
    assert(m_cpuStateSyncTables.size() == (rt_dump_tb_count - m_streamed_tb_count) &&
               "Something wrong in m_cpuStateSyncTables dump, its size should be equal to rt_dump_tb_count all the time.\n");
    assert(m_debug_cpuStateSyncTables.size() == (rt_dump_tb_count - m_streamed_tb_count) &&
               "Something wrong in m_cpuStateSyncTables dump, its size should be equal to rt_dump_tb_count all the time.\n");

    assert(m_memoSyncTables.size() == (rt_dump_tb_count - m_streamed_tb_count) &&
                "Something wrong in m_memoSyncTables dump, its size should be equal to rt_dump_tb_count all the time.\n");

    assert(m_interruptStates.size() == (rt_dump_tb_count) &&
    		"Something wrong in m_interruptStates dump, its size should be equal to (rt_dump_tb_count - 1) all the time.\n");

#if defined(CRETE_DBG_MEM_MONI)
    assert(m_debug_memoSyncTables.size() == (rt_dump_tb_count) &&
            "Something wrong in m_debug_memoSyncTables dump, its size should be equal to rt_dump_tb_count all the time.\n");
    assert(m_debug_memoMergePoints.size() == (rt_dump_tb_count) &&
            "Something wrong in m_debug_memoSyncTables dump, its size should be equal to rt_dump_tb_count all the time.\n");
#endif //CRETE_DBG_MEM_MONI
}

string RuntimeEnv::get_tcoHelper_name(uint64_t func_addr) const
{

    map<uint64_t, string>::const_iterator it = m_debug_helper_names.find(func_addr);
    assert(it != m_debug_helper_names.end());
    return it->second;
}

void RuntimeEnv::init_debug_cpuState_offsets(const vector<pair<string, pair<uint64_t, uint64_t> > >& offsets)
{
    assert(!offsets.empty());

    for(vector<pair<string, pair<uint64_t, uint64_t> > >::const_iterator it = offsets.begin();
            it != offsets.end(); ++ it) {
        if( !(m_debug_cpuState_offsets.insert(make_pair(it->first, make_pair(it->second.first, it->second.second))).second)){
            fprintf(stderr, "insert %s failed in init_debug_cpuState_offsets()\n",
                    it->first.c_str());
        }
    }
}

void RuntimeEnv::set_dbgCPUStatePostInterest(const void *src)
{
    assert(src);
    memcpy(m_dbg_cpuState_post_interest, src,
            sizeof(CPUArchState));
}

void RuntimeEnv::check_dbgCPUStatePostInterest(const void *src)
{
    vector<CPUStateElement> differs = x86_cpuState_compuate_side_effect((const CPUArchState *)m_dbg_cpuState_post_interest,
            (const CPUArchState *)src);
    if(differs.empty())
    {
        return;
    }

    cerr << "[CRETE ERROR] CPUState is changed after an interested tb being dumped and before the next tb starts to execute\n";
    uint8_t* post_interest_cpuState = (uint8_t*)m_dbg_cpuState_post_interest;
    uint8_t* current_cpuState = (uint8_t*)src;

    for(vector<CPUStateElement>::const_iterator it = differs.begin();
            it != differs.end(); ++it) {
        cerr << it->m_name << ": " << it->m_size << " bytes\n";
        cerr << " post_interest_value :[";
        for(uint64_t i = 0; i < it->m_size; ++i) {
            cerr << " 0x"<< hex << (uint32_t)*(post_interest_cpuState + it->m_offset + i);
        }
        cerr << "]\n";

        cerr << " current_value :[";
        for(uint64_t i = 0; i < it->m_size; ++i) {
            cerr << " 0x"<< hex << (uint32_t)*(current_cpuState + it->m_offset + i);
        }

        cerr << "]\n";
    }
}

/********************************************************/
/* Private methods for internal usage within RunteimEnv */

void RuntimeEnv::init_concolics()
{
    using namespace crete;

    assert(boost::filesystem::exists("hostfile/input_arguments.bin"));
    ifstream inputs("hostfile/input_arguments.bin", ios_base::in | ios_base::binary);

    assert(inputs && "failed to open input_arguments.bin!");

    if(empty_test_case(inputs)){
        fprintf(stderr, "[CRETE WARNING] RuntimeEnv::init_concolics(): empty test case from hostfile/input_arguments.bin\n");
        return;
    }

    inputs.clear();
    inputs.seekg(0, ios::beg);
    TestCase tc = read_test_case(inputs);

    for(vector<TestCaseElement>::const_iterator tc_iter = tc.get_elements().begin();
        tc_iter !=  tc.get_elements().end();
        ++tc_iter)
    {
        uint32_t size = tc_iter->data_size;
        string name(tc_iter->name.begin(), tc_iter->name.end());
        vector<uint8_t> data = tc_iter->data;

        assert(size == data.size());
        assert(name.length() == tc_iter->name_size);

        m_concolics.insert(make_pair(name, CreteMemoInfo(size, name, data)));
    }
}

void RuntimeEnv::dump_tloTbPc(const uint64_t pc)
{
	m_tcg_llvm_offline_ctx.dump_tlo_tb_pc(pc);
}

void RuntimeEnv::dump_tloTcgCtx(const TCGContext &tcg_ctx)
{
	m_tcg_llvm_offline_ctx.dump_tcg_ctx(tcg_ctx);
}

void RuntimeEnv::dump_tloHelpers(const TCGContext &tcg_ctx)
{
	m_tcg_llvm_offline_ctx.dump_tcg_helper_name(tcg_ctx);

	m_debug_helper_names = m_tcg_llvm_offline_ctx.get_helper_names();
}

void RuntimeEnv::dump_tcg_temp(const vector<TCGTemp> &tcg_temp)
{
	m_tcg_llvm_offline_ctx.dump_tcg_temp(tcg_temp);
}

void RuntimeEnv::dump_tloTbInstCount(const uint64_t inst_count)
{
	m_tcg_llvm_offline_ctx.dump_tlo_tb_inst_count(inst_count);
}

void RuntimeEnv::writeTcgLlvmCtx()
{
    stringstream ss;
    ss << "dump_tcg_llvm_offline." << m_streamed_index << ".bin";
    ofstream ofs(getOutputFilename(ss.str()).c_str(), ios_base::binary);

    assert(ofs.good());
	try {
        boost::archive::binary_oarchive oa(ofs);
	    oa << m_tcg_llvm_offline_ctx;
	}
	catch(std::exception &e){
	    cerr << e.what() << endl;
	}

    m_tcg_llvm_offline_ctx = TCGLLVMOfflineContext();
}

string RuntimeEnv::getOutputFilename(const string &fileName) const
{
    fs::path filePath(m_outputDirectory);
    filePath /= fileName;
    return filePath.string();
}

void RuntimeEnv::writeConcolics()
{
    assert(!m_concolics.empty());
    assert(m_concolics.size() == m_make_concolic_order.size());

    ofstream o_fs(getOutputFilename("dump_mo_symbolics.txt").c_str());
    assert(o_fs.good());

    crete::TestCase tc;

    for(vector<string>::iterator c_it = m_make_concolic_order.begin();
        c_it != m_make_concolic_order.end(); ++c_it)
    {
        creteConcolics_ty::iterator it = m_concolics.find(*c_it);
        assert(it != m_concolics.end());
        assert(it->second.m_addr_valid);

        // TODO: reduce field number
        // New/symbolic format:
        o_fs << it->second.m_name
                << ' '
                << '0'
                << ' '
                << '0'
                << ' '
                << it->second.m_size
                << ' '
                << it->second.m_addr
                << ' '
                << '0'
                << '\n';

        CreteMemoInfo crete_memo = it->second;
        crete::TestCaseElement tce;
        tce.name = vector<uint8_t>(crete_memo.m_name.begin(), crete_memo.m_name.end());
        tce.name_size = crete_memo.m_name.size();
        tce.data = crete_memo.m_data;
        tce.data_size = crete_memo.m_size;
        tc.add_element(tce);
    }

    // Update "hostfile/input_arguments.bin" as there are more concolics than specified in xml
    ofstream ofs("hostfile/input_arguments.bin", ios_base::out | ios_base::binary);
    assert(ofs);
    tc.write(ofs);
}

void RuntimeEnv::setCPUStatePostInterest(const void *src)
{
    assert(src);
    memcpy(m_cpuState_post_insterest.second, src,
            sizeof(CPUArchState));
}

void RuntimeEnv::setFlagCPUStatePostInterest()
{
    assert(m_cpuState_post_insterest.first == false &&
            "m_cpuState_post_insterest should always be invalided in addcpuStateSyncTable().\n");

    m_cpuState_post_insterest.first = true;
}

void RuntimeEnv::setCPUStatePreInterest(const void *src)
{
    assert(src);
    memcpy(m_cpuState_pre_interest.second, src,
            sizeof(CPUArchState));
    m_cpuState_pre_interest.first = true;
}

void RuntimeEnv::resetCPUStatePreInterest()
{
    m_cpuState_pre_interest.first = false;
}

void RuntimeEnv::writeMemoSyncTables()
{
    stringstream ss;
    ss << "dump_new_sync_memos." << m_streamed_index << ".bin";
    ofstream o_sm(getOutputFilename(ss.str()).c_str(), ios_base::binary);

    assert(o_sm.good());

    // boost::unordered_map is not supported by boost::serialization
    // Covert boost::unordered_map to vector< pair<> >
    vector<vector<pair<uint64_t, uint8_t> > > to_serialize;
    to_serialize.reserve(m_memoSyncTables.size());

    for(memoSyncTables_ty::const_iterator it = m_memoSyncTables.begin();
            it != m_memoSyncTables.end(); ++it) {
        vector<pair<uint64_t, uint8_t> > temp_memSyncTable;
        temp_memSyncTable.reserve(it->size());

        for(memoSyncTable_ty::const_iterator in_it = it->begin();
                in_it != it->end(); ++in_it) {
            temp_memSyncTable.push_back(*in_it);
        }

        assert(temp_memSyncTable.size() == it->size());
        to_serialize.push_back(temp_memSyncTable);
    }

    assert(to_serialize.size() == m_memoSyncTables.size());

    try {
        boost::archive::binary_oarchive oa(o_sm);
        oa << to_serialize;
    }
    catch(std::exception &e){
        cerr << e.what() << endl;
    }

    m_memoSyncTables.clear();
}

#if defined(CRETE_DBG_MEM_MONI)
// merge a sequence of consecutive non-BackToInterestTb TB's memoSyncTables
//  to the nearest BackToInterestTb TB's memoSyncTable and make them empty
void RuntimeEnv::debug_mergeMemoSyncTables()
{
    debugMergeMemoSync();

    assert(m_debug_memoSyncTables.size() == (rt_dump_tb_count));
    assert(m_debug_memoMergePoints.size() == (rt_dump_tb_count));

    assert(m_debug_memoMergePoints.front() == BackToInterestTb ||
            m_debug_memoMergePoints.front() == OutAndBackTb);
    assert(m_debug_memoMergePoints.back() == OutofInterestTb ||
            m_debug_memoMergePoints.back() == OutAndBackTb);

    debug_memoSyncTable_ty *dst_mst = 0;
    debug_memoSyncTable_ty *src_mst = 0;

    dst_mst = &m_debug_memoSyncTables[0];
    for(uint64_t i = 1; i < m_debug_memoSyncTables.size(); ++i) {
        if(m_debug_memoMergePoints[i] == BackToInterestTb) {
            // the BackToInterestTb TB's memoSyncTable will be used as destination for the merging
            dst_mst = &m_debug_memoSyncTables[i];
            continue;
        } else if ( m_debug_memoMergePoints[i] == OutAndBackTb) {
            dst_mst = 0;
            continue;
        }

        // the NormalTb and OutofInterestTb TB's memoSyncTable will be used as source and will be cleared
        assert(m_debug_memoMergePoints[i] == NormalTb ||
                m_debug_memoMergePoints[i] == OutofInterestTb);

        src_mst = &m_debug_memoSyncTables[i];
        assert(dst_mst && src_mst);

        //insert source memorySyncTable to destination memorySyncTable
        for(debug_memoSyncTable_ty::iterator it = src_mst->begin();
                        it != src_mst->end(); ++it){
            uint64_t src_entry_addr = it->second.m_addr;
            uint64_t src_entry_size = it->second.m_size;
            vector<uint8_t> src_entry_value = it->second.m_data;
            addMemoSyncTableEntryInternal(src_entry_addr, src_entry_size, src_entry_value, *dst_mst);
        }

        src_mst->clear();
        assert(m_debug_memoSyncTables[i].empty());
    }
}

void RuntimeEnv::debug_writeMemoSyncTables()
{
#if defined(CRETE_DEBUG)
    cerr << "Verifying all m_debug_memoSyncTables before merging memoSyncTables: \n";
    for(debug_memoSyncTables_ty::const_iterator const_it = m_debug_memoSyncTables.begin();
            const_it != m_debug_memoSyncTables.begin(); ++const_it) {
        verifyMemoSyncTable(*const_it);
    }
#endif

    debug_mergeMemoSyncTables();

#if defined(CRETE_DEBUG)
    cerr << "[m_debug_memoMergePoints] " << endl;
    for(uint64_t i = 0; i < m_debug_memoMergePoints.size(); ++i) {
        cerr << "TB_" << i <<": Merge Type = " << m_debug_memoMergePoints[i]
             << ", memoSyncTable size = " << m_debug_memoSyncTables[i].size() << endl;
    }
#endif // defined(CRETE_DEBUG)

    // double check the merge is correct
    for(uint64_t i = 0; i < m_debug_memoMergePoints.size(); ++i) {
        if(m_debug_memoMergePoints[i] == OutofInterestTb ||
                m_debug_memoMergePoints[i] == NormalTb) {
            assert(m_debug_memoSyncTables[i].empty() && "Something is wrong in debug_mergeMemoSyncTables().\n");
            assert(m_memoSyncTables[i].empty());
        }
    }

    ofstream o_sm(getOutputFilename("dump_sync_memos.bin").c_str(),
                ios_base::binary);
    assert(o_sm && "Create file failed: dump_sync_memos.bin\n");

    try {
        boost::archive::binary_oarchive oa(o_sm);
        oa << m_debug_memoSyncTables;
    }
    catch(std::exception &e){
        cerr << e.what() << endl;
    }

#if defined(CRETE_DBG_TA)
    print_memoSyncTables();
#endif

    m_debug_memoSyncTables.clear();
    m_debug_memoMergePoints.clear();
}

void RuntimeEnv::addMemoSyncTableEntryInternal(uint64_t addr, uint32_t size, vector<uint8_t> v_value,
         debug_memoSyncTable_ty& target_memoSyncTable)
{
    assert(v_value.size() == size);

    //Find the existing entries that overlap with the new entry
    vector<uint64_t> overlapped_entries= overlapsMemoSyncEntry(addr, size, target_memoSyncTable);

    // If there is no overlaps, just insert the new entry
    if(overlapped_entries.empty()){
        CreteMemoInfo entry_memoSyncTable(addr, size, v_value);
        assert(!m_debug_memoSyncTables.empty());

        target_memoSyncTable.insert(pair<uint64_t, CreteMemoInfo>(addr, entry_memoSyncTable));
    } else {
        // If there are overlaps, combine them, including the new given one, to one entry while keeping values
        // of the overlapped bytes from existing entries

        // The address of the new merged entry should be the smallest address
        uint64_t first_overlapped_addr = overlapped_entries.front();
        uint64_t new_merged_addr = (first_overlapped_addr < addr) ?
                first_overlapped_addr : addr;

        uint64_t last_overlapped_addr = overlapped_entries.back();
        uint64_t last_overlapped_size = target_memoSyncTable.find(last_overlapped_addr)->second.m_size;
        uint64_t new_merged_end_addr = ((last_overlapped_addr + last_overlapped_size) > (addr + size) ) ?
                (last_overlapped_addr + last_overlapped_size) : (addr + size);

        uint32_t new_merged_size = new_merged_end_addr - new_merged_addr;
        vector<uint8_t> new_merged_value;

//      cerr << "new_merged_addr = 0x" << hex << new_merged_addr
//              << ", new_merged_size = 0x" << new_merged_size << endl;

        // Write values for new merged entry
        uint64_t current_writing_byte_addr = new_merged_addr; // The address of the byte that is writing to
        for(vector<uint64_t>::iterator it = overlapped_entries.begin();
                it != overlapped_entries.end(); ++it) {
            debug_memoSyncTable_ty::iterator it_memo_entry = target_memoSyncTable.find(*it);
            assert(it_memo_entry != target_memoSyncTable.end());
            uint64_t exist_entry_addr = it_memo_entry->second.m_addr;
            uint32_t exist_entry_size = it_memo_entry->second.m_size;
            const vector<uint8_t>& exist_entry_value = it_memo_entry->second.m_data;

//          cerr << "1. exist entry: (0x" << hex << exist_entry_addr << ", 0x" << exist_entry_size << ")" << endl;

            // If the current_writing_byte_addr is smaller, that means we need to get the missing bytes from
            // the new given entry
            if (current_writing_byte_addr != exist_entry_addr) {
//              cerr << "2.1 Get value from new entry.\n";
                assert(current_writing_byte_addr < exist_entry_addr);
                while(current_writing_byte_addr < exist_entry_addr){
                    uint32_t offset_given_entry = current_writing_byte_addr - addr;
                    new_merged_value.push_back(v_value[offset_given_entry]);
                    ++current_writing_byte_addr;
                }
            }

//          cerr << "2.2 Get value from exist entry.\n";
            assert(current_writing_byte_addr == exist_entry_addr);
            while(current_writing_byte_addr < exist_entry_addr + exist_entry_size){
                int64_t offset_exist_entry = current_writing_byte_addr - exist_entry_addr;
                assert(offset_exist_entry >= 0 && offset_exist_entry < exist_entry_size);
                new_merged_value.push_back(exist_entry_value[offset_exist_entry]);
                ++current_writing_byte_addr;
            }

//          cerr << "3. Erase" << endl;
            // Delete the overlapped entry
            target_memoSyncTable.erase(it_memo_entry);
        }

        if(current_writing_byte_addr < new_merged_end_addr) {
            assert( current_writing_byte_addr < (addr + size) );

            while( current_writing_byte_addr < (addr + size) ){
                uint32_t offset_given_entry = current_writing_byte_addr - addr;
                new_merged_value.push_back(v_value[offset_given_entry]);
                ++current_writing_byte_addr;
            }
        }

//      cerr << "4. check for addMemoSyncTableEntryInternal\n";

        assert(new_merged_value.size() == new_merged_size);
        assert(overlapsMemoSyncEntry(addr, size, target_memoSyncTable).empty() &&
                " There should be no overlapped entries anymore after the merge" );
        // Insert the new merged entry to
        CreteMemoInfo entry_memoSyncTable(new_merged_addr, new_merged_size, new_merged_value);
        assert(!m_debug_memoSyncTables.empty());

        target_memoSyncTable.insert(pair<uint64_t, CreteMemoInfo>(new_merged_addr, entry_memoSyncTable));
    }
}

void RuntimeEnv::verifyMemoSyncTable(const debug_memoSyncTable_ty& target_memoSyncTable)
{
    for(debug_memoSyncTable_ty::const_iterator it = target_memoSyncTable.begin();
            it != target_memoSyncTable.end(); ++it) {
        if(it->first != it->second.m_addr) {
            cerr << hex << "it->first = 0x" << it->first << "it->second.m_addr = 0x" << it->second.m_addr << endl;
            assert(0 && "verifyMemoSyncTable() failed.\n");
        }
    }
}

void RuntimeEnv::debugMergeMemoSync()
{
#if defined(CRETE_DEBUG)
    cerr << "[debugMergeMemoSync]" << endl
            << "m_tbExecSequ.size() = " << m_tbExecSequ.size() << ", "
            << "m_debug_memoMergePoints.size() = " << m_debug_memoMergePoints.size() << ", "
            << "m_debug_memoSyncTables.size() = " << m_debug_memoSyncTables.size() << endl;

    cerr << "\t m_debug_memoMergePoints type: ";

    for(uint64_t i = 0; i < m_debug_memoMergePoints.size(); ++i) {
        cerr << m_debug_memoMergePoints[i] << " ";
    }
    cerr << "\n\t m_debug_memoSyncTables[i].size(): ";
    for(uint64_t i = 0; i < m_debug_memoSyncTables.size(); ++i) {
        cerr << m_debug_memoSyncTables[i].size() << " ";
    }
    cerr << endl;
#endif // defined(CRETE_DEBUG)
}

void RuntimeEnv::print_memoSyncTables()
{
    cerr << "[memoSyncTables:]\n";
    for(uint64_t temp_tb_count = 0; temp_tb_count < m_debug_memoSyncTables.size();
            ++temp_tb_count){
        if(m_memoSyncTables[temp_tb_count].empty()){
            assert(m_debug_memoSyncTables[temp_tb_count].empty());
            cerr << "===================================================================\n";
            cerr << "tb_count: " << dec << temp_tb_count<< ": NULL\n";
            cerr << "===================================================================\n";
        } else {
            assert(!m_debug_memoSyncTables[temp_tb_count].empty());
            cerr << "===================================================================\n";
            cerr << "tb_count: " << dec << temp_tb_count << endl;
            cerr << "===================================================================\n";

            cerr << "memoSyncTables size = " << m_memoSyncTables[temp_tb_count].size() << "\n";
            for(memoSyncTable_ty::iterator m_it = m_memoSyncTables[temp_tb_count].begin();
                    m_it != m_memoSyncTables[temp_tb_count].end(); ++m_it){
                cerr << hex << "0x" << m_it->first << ": (0x " << (uint64_t)m_it->second  << "); ";
            }

            cerr << "---------------------------------------------------------------------\n"
                    << "m_debug_memoSyncTables size:" << m_debug_memoSyncTables[temp_tb_count].size()
                    << endl;
            for(debug_memoSyncTable_ty::iterator m_it = m_debug_memoSyncTables[temp_tb_count].begin();
                    m_it != m_debug_memoSyncTables[temp_tb_count].end(); ++m_it){
                cerr << hex << "0x" << m_it->first << ": (0x " << m_it->second.m_size <<", 0x ";
                for(vector<uint8_t>::iterator uint8_it =  m_it->second.m_data.begin();
                        uint8_it !=  m_it->second.m_data.end(); ++uint8_it){
                    cerr << (uint32_t)(*uint8_it) << " ";
                }
                cerr << ")" << "; ";
            }
            cerr << dec << endl;
        }
    }
}
#endif //#if defined(CRETE_DBG_MEM_MONI)


void RuntimeEnv::writeInitialCpuState()
{
    assert(m_initial_CpuState.size() == sizeof(CPUArchState));

    ofstream o_sm(getOutputFilename("dump_initial_cpuState.bin").c_str(),
                ios_base::binary);
    assert(o_sm && "Create file failed: dump_initial_cpuState.bin\n");
    o_sm.write((const char*)m_initial_CpuState.data(), sizeof(CPUArchState));
    o_sm.clear();

    m_initial_CpuState.clear();
}

void RuntimeEnv::writeDebugCpuStateOffsets()
{
    ofstream o_sm(getOutputFilename("dump_debug_cpuState_offsets.bin").c_str(),
                ios_base::binary);
    assert(o_sm && "Create file failed: dump_debug_cpuState_offsets.bin\n");

    try {
        boost::archive::binary_oarchive oa(o_sm);
        oa << m_debug_cpuState_offsets;
    }
    catch(std::exception &e){
        cerr << e.what() << endl;
    }

    m_debug_cpuState_offsets.clear();
}


void RuntimeEnv::checkEmptyCPUStateSyncTables()
{
    uint64_t tb_count = 0;
    for(vector<cpuStateSyncTable_ty>::iterator it = m_cpuStateSyncTables.begin();
            it != m_cpuStateSyncTables.end(); ++it) {
        if(it->first && it->second.empty()) {
            it->first = false;

#if defined(CRETE_DBG_CK)
            fprintf(stderr, "CPUState is not changed between tb-%lu, and tb-%lu\n",
                    tb_count - 1, tb_count);
#endif
        }

        ++tb_count;
    }
}

void RuntimeEnv::writeCPUStateSyncTables()
{
    checkEmptyCPUStateSyncTables();

    stringstream ss;
    ss << "dump_sync_cpu_states." << m_streamed_index << ".bin";
    ofstream o_sm(getOutputFilename(ss.str()).c_str(), ios_base::binary);

    assert(o_sm.good() && "Create file failed: dump_sync_cpu_states.bin\n");

    try {
        boost::archive::binary_oarchive oa(o_sm);
        oa << m_cpuStateSyncTables;
    }
    catch(std::exception &e){
        cerr << e.what() << endl;
    }

    m_cpuStateSyncTables.clear();
}

void RuntimeEnv::writeDebugCPUStateSyncTables()
{
    stringstream ss;
    ss << "dump_debug_sync_cpu_states." << m_streamed_index << ".bin";
    ofstream o_sm(getOutputFilename(ss.str()).c_str(), ios_base::binary);

    assert(o_sm && "Create file failed: dump_debug_sync_cpu_states.bin\n");

    try {
        boost::archive::binary_oarchive oa(o_sm);
        oa << m_debug_cpuStateSyncTables;
    }
    catch(std::exception &e){
        cerr << e.what() << endl;
    }

    m_debug_cpuStateSyncTables.clear();
}

// Check whether the given entry (addr, size) overlaps with existing entries in target_memoSyncTable
// Return the the address of all the overlapped entries
vector<uint64_t> RuntimeEnv::overlapsMemoSyncEntry(uint64_t addr, uint32_t size,
        debug_memoSyncTable_ty target_memoSyncTable)
{
#if defined(CRETE_DEBUG)
    cerr << "[Ld Memo Monitor] overlapsMemoSyncEntry() is invoked."
            << "(0x " << hex << addr << ", 0x " << dec << size << ")" << endl;
    cerr << "Exsiting Entries: target_memoSyncTable.size() = " << dec << target_memoSyncTable.size() << endl;
    for(debug_memoSyncTable_ty::iterator temp_it = target_memoSyncTable.begin();
            temp_it != target_memoSyncTable.end(); ++temp_it){
        cerr << hex << "(0x " << temp_it->second.m_addr << ", 0x" << temp_it->second.m_size << "), ";
    }
    cerr << endl;
#endif // defined(CRETE_DEBUG)

    if(target_memoSyncTable.empty())
        return vector<uint64_t>();

    // Find the last entry that goes before given address, "last before" entry, if found
    // If all the entries goes after the given address, it_last_before points to the first entry
    debug_memoSyncTable_ty::iterator it_last_before = target_memoSyncTable.begin();
    for(debug_memoSyncTable_ty::iterator temp_it = target_memoSyncTable.begin();
            temp_it != target_memoSyncTable.end(); ++temp_it) {
        if(temp_it->first < addr) {
            it_last_before = temp_it;
//          cerr << "entry goes before is found: (0x " << it_last_before->second.m_addr
//                  << ", 0x " << it_last_before->second.m_size << ")\n";
        } else {
            break;
        }
    }

#if defined(CRETE_DEBUG)
    cerr << "it_last_before->first: (0x " << hex << it_last_before->first
            << ", 0x " << it_last_before->second.m_addr
            << ", 0x " << it_last_before->second.m_size << ")"<< endl;
#endif

    vector<uint64_t> ret_addrs;
    // Check the coming entries one by one to see whether they overlaps with the given entry
    for(debug_memoSyncTable_ty::iterator it_temp = it_last_before;
            it_temp !=  target_memoSyncTable.end(); ++it_temp){
        // if the current one overlaps, return it
        if( (it_temp->second.m_addr + it_temp->second.m_size > addr) &&
                (it_temp->second.m_addr < addr + size) ) {
            ret_addrs.push_back(it_temp->second.m_addr);

//          cerr << "overlaps: addr = 0x" << hex << it_temp->second.m_addr
//                  << ", size = " << dec <<  it_temp->second.m_size << endl;
        }

        if(it_temp->second.m_addr >= (addr + size)){
            break;
        }
    }

#if defined(CRETE_DEBUG)
    cerr << "[Ld Memo Monitor] overlapsMemoSyncEntry() is finished. ret_addrs.size() = "
            << hex << ret_addrs.size()<< endl;
#endif // defined(CRETE_DEBUG)

    return ret_addrs;
}

void RuntimeEnv::writeInterruptStates() const
{
    assert(m_interruptStates.size() == rt_dump_tb_count);

    ofstream o_sm(getOutputFilename("dump_qemu_interrupt_info.bin").c_str(),
            ios_base::binary);
    assert(o_sm && "Create file failed: dump_qemu_interrupt_info.bin\n");

    try {
        boost::archive::binary_oarchive oa(o_sm);
        oa << m_interruptStates;
    }
    catch(std::exception &e){
        cerr << e.what() << endl;
    }
}

void RuntimeEnv::addTBGraphExecSequ(uint64_t tb_pc)
{
    m_tbGraphExecSequ.push_back(tb_pc);
}

void RuntimeEnv::writeTBGraphExecSequ()
{
    string path = getOutputFilename("tb-seq.bin");
    ofstream ofs(path.c_str(), ios_base::out | ios_base::binary);
    if(!ofs.good())
        throw runtime_error("can't open file: " + path);

    for(vector<uint64_t>::iterator iter = m_tbGraphExecSequ.begin();
        iter != m_tbGraphExecSequ.end();
        ++iter)
    {
        ofs.write(reinterpret_cast<const char*>(&*iter),
                  sizeof(uint64_t));
    }

    path = getOutputFilename("tb-seq.txt");
    ofstream ofs_txt(path.c_str(), ios_base::out);
    if(!ofs_txt.good())
        throw runtime_error("can't open file: " + path);

    for(vector<uint64_t>::const_iterator iter = m_tbExecSequPC.begin();
        iter != m_tbExecSequPC.end();
        ++iter)
    {
        ofs_txt << hex << (*iter) << endl;
    }
}

CreteFlags::CreteFlags()
: m_cpuState(NULL), m_tb(NULL),
  m_target_pid(0), m_capture_started(false),
  m_capture_enabled(false), m_valid(false) {}

CreteFlags::~CreteFlags(){}

void CreteFlags::set(uint64_t target_pid)
{
	m_target_pid = target_pid;
	m_capture_started = true;
	m_capture_enabled = true;

	m_valid = true;
}

void CreteFlags::reset()
{
	m_target_pid = 0;
	m_capture_started = false;
	m_capture_enabled = false;

	m_valid = false;
}

void CreteFlags::set_capture_enabled(bool capture_enabled)
{
	m_capture_enabled = capture_enabled;
}

bool CreteFlags::is_true() const
{
	assert(m_cpuState && m_tb);

	return  m_valid &&
			m_capture_started &&
			m_capture_enabled &&
			(m_target_pid == ((CPUArchState *)m_cpuState)->cr[3]) &&
			(m_tb->pc < USER_CODE_RANGE);
}

bool CreteFlags::is_true(void* cpuState, TranslationBlock *tb) const
{
	assert(cpuState && tb);

	return  m_valid &&
			m_capture_started &&
			m_capture_enabled &&
			(m_target_pid == ((CPUArchState *)cpuState)->cr[3]) &&
			(tb->pc < USER_CODE_RANGE);
}

void CreteFlags::check(bool valid) const
{
	assert(m_valid == valid && "[CRETE Error] CreteFlags::check() failed.\n");
}
/*****************************/
/* Functions for QEMU c code */

void crete_runtime_dump_initialize()
{
    runtime_env = 0;
    rt_dump_tb = 0;
    rt_dump_tb_count = 0;
    nb_captured_llvm_tb = 0;
    flag_rt_dump_enable = 0;
    flag_interested_tb = 0;
    flag_interested_tb_prev = 0;

    is_begin_capture = 0;
    is_target_pid = 0;
    is_user_code = 0;

    g_crete_flags = new CreteFlags;

    runtime_env = new RuntimeEnv;

    assert(runtime_env && g_crete_flags);

#if defined(CRETE_CROSS_CHECK)
    crete_verify_cpuState_offset_c_cxx();
#endif
}

void crete_runtime_dump_close()
{
    if(runtime_env) {
        delete runtime_env;
        runtime_env = NULL;
    }

    if(g_crete_flags) {
    	delete g_crete_flags;
    	g_crete_flags = NULL;
    }
}

void crete_pre_cpu_tb_exec(void *qemuCpuState, TranslationBlock *tb)
{
    crete_pre_post_flag = true;

    CPUArchState *env = (CPUArchState *)qemuCpuState;

    // check assumptions we made by assertions
    assert(is_target_pid == (env->cr[3] == g_crete_target_pid) &&
            "env->cr[3] should stay the same within one iteration of inner loop of cpu_exec()\n");
    assert(!use_icount);

    // set globals for debug using
    g_cpuState_bct = env;
    rt_dump_tb = tb;

    is_begin_capture = (g_custom_inst_emit == 1);

    if(!is_begin_capture)
        return;

//  if (flag_rt_dump_enable)
//      printf("[Warning]: \"cpu-exec.c\" flag_rt_dump_enable = 1 before a TB is executed.\n");

    //1. Set flags related for runtime dump
    flag_rt_dump_enable = 0;
    flag_interested_tb_prev = flag_interested_tb;

    // set flags related to code selection
    is_target_pid = (env->cr[3] == g_crete_target_pid);
    is_user_code = (tb->pc < USER_CODE_RANGE);

//    bool is_in_include_filter = crete_is_pc_in_include_filter_range(tb->pc);
    bool is_in_exclude_filter = crete_is_pc_in_exclude_filter_range(tb->pc);

    // 2. set flag of filter TB based on above flags (taint analysis)
    int is_interested_tb =
    		is_begin_capture &&
			is_target_pid &&
			is_user_code &&
			!is_in_exclude_filter;

#if defined(CRETE_DBG_CK)
//    if(is_begin_capture && is_target_pid && is_user_code)
    if(is_interested_tb)
    {
        std::cerr << "\n[PRE] TB-PC = " << (void *)(uint64_t)tb->pc << std::endl;

        if(is_in_list_crete_dbg_tb_pc(tb->pc)){
            fprintf(stderr, "crete_pre_cpu_tb_exec(): ");

            {
                uint64_t i;
                const uint8_t *ptr_addr = (const uint8_t*)(&env->CRETE_DBG_REG);
                uint8_t byte_value;
                fprintf(stderr, "env->" CRETE_GET_STRING(CRETE_DBG_REG) " = [");
                for(i = 0; i < sizeof(env->CRETE_DBG_REG); ++i) {
                    byte_value = *(ptr_addr + i);
                    fprintf(stderr, " 0x%02x", byte_value);
                }
                fprintf(stderr, "]\n");
            }
        }
    }

    CRETE_DBG_INT(
    if(is_begin_capture && is_target_pid)
    {
        runtime_env->check_dbgCPUStatePostInterest(qemuCpuState);
    }
    );
#endif

    // Set flags: flag_rt_dump_start/ flag_rt_dump_enable/ flag_interested_tb
    if(is_interested_tb)
    {
        /* enable flg_rt_dump_enable */
        flag_rt_dump_enable = 1;

        flag_interested_tb = 1;
    } else {
        flag_interested_tb = 0;
    }

#if defined(CRETE_DBG_MEM_MONI)
    // 4. Memory Monitor
    if(flag_interested_tb_prev == 0 && flag_interested_tb == 1) {
        runtime_env->addMemoMergePoint(BackToInterestTb);
    } else if(flag_interested_tb_prev == 1 && flag_interested_tb == 0) {
        runtime_env->addMemoMergePoint(OutofInterestTb);
    } else if(flag_interested_tb_prev == 1 && flag_interested_tb == 1) {
        runtime_env->addMemoMergePoint(NormalTb);
    }
#endif

    /* 5. runtime information dumped before the exeuction of current TB
     * Including:
     *      3. empty SyncTable for Memory Monitor
     */
    if(flag_rt_dump_enable) {
        //3. keep a copy of the cpuState before the execution of a potential insterested tb
        runtime_env->setCPUStatePreInterest((void *)env);

        //4. MemosyncTable
#if defined(CRETE_DBG_MEM_MONI)
        runtime_env->addMemoSyncTable();
#endif
        runtime_env->clearCurrentMemoSyncTable();

        ++rt_dump_tb_count;
    } else {
        runtime_env->resetCPUStatePreInterest();
    } // if(flag_rt_dump_enable)
}

// Ret: whether the current tb is interested after post_runtime_dump
// @next_tb: a pointer to the next TB to execute
// (if known; otherwise zero). This pointer is assumed to be
// 4-aligned, and the bottom two bits are used to return further
// information. Check tcg.h for details.
// @crete_interrupted_pc: when non-zero, the pc value being interrupted within the execution
//  of the current tb
int crete_post_cpu_tb_exec(void *qemuCpuState, TranslationBlock *input_tb, uint64_t next_tb,
        uint64_t crete_interrupted_pc)
{
    assert(crete_pre_post_flag);
    crete_pre_post_flag = false;
    CRETE_DBG_INT(
    if(is_begin_capture && is_target_pid)
    {
        runtime_env->set_dbgCPUStatePostInterest(qemuCpuState);
    }
    );

	if(!flag_interested_tb) {
	    // Set runtime_env->m_cpuState_post_insterest
	    if(flag_interested_tb_prev == 1 && flag_interested_tb == 0) {
	        runtime_env->setFlagCPUStatePostInterest();
	    }

	    return 0;
	}

	assert(flag_interested_tb);

	TranslationBlock tb;
	memcpy(&tb, input_tb, sizeof(TranslationBlock));

	int is_post_interested_tb = 0;
    bool is_current_tb_executed = ((next_tb & TB_EXIT_MASK) <= TB_EXIT_IDX1) ? true:false;

    //reverse runtime tracing: initial cpuState/memory/Regs, if:
    // 1. the current tb was not executed OR
    // 2. the current tb does not have symbolic operations
    // 3. the current tb was interrupted at the first instruction
    bool reverse = !is_current_tb_executed || !crete_tci_is_current_block_symbolic() ||
            (tb.pc == crete_interrupted_pc);
//    bool reverse = !is_current_tb_executed || (!crete_tci_is_current_block_symbolic() &&
//            !crete_tci_is_previous_block_symbolic());

    if(reverse) {
        flag_interested_tb = 0;
    	flag_rt_dump_enable = 0;
    	--rt_dump_tb_count;
    	runtime_env->reverseTBDump(qemuCpuState);

    	is_post_interested_tb = 0;

    	// NOTE: streaming tracing only being called after the execution of a un-interested TB
    	//       for the favor of memory-monitoring (MM needs to do merge)
    	runtime_env->stream_writeRtEnvToFile(rt_dump_tb_count);
    } else {
    	// Otherwise, finish-up tracing: instruction sequence, translation context, interrupt states,
        // and CPUStateSyncTable
        if(rt_dump_tb_count == 1) {
            runtime_env->addInitialCpuState();
        }

        // 1. instruction sequence and its translation context
        if(crete_interrupted_pc != 0){
            assert(runtime_env != NULL);
            runtime_env->dump_tloCtx(qemuCpuState, &tb, crete_interrupted_pc);
        } else if(tb.tcg_ctx_captured == 0){
          assert(runtime_env != NULL);
          runtime_env->dump_tloCtx(qemuCpuState, &tb, 0);

          // Set flag for caching traced tcg_ctx
          input_tb->tcg_ctx_captured = 1;
          input_tb->index_captured_llvm_tb = tb.index_captured_llvm_tb;
        }

        runtime_env->addTBExecSequ(tb.index_captured_llvm_tb, tb.pc);

        // 2. CPUState tracing:
        // 2.1 calculate CPUState side-effects for TBs that are "BackToInterestTb"
        if(rt_dump_tb_count == 1) {
            // add empty table for the first interested tb
            runtime_env->addEmptyCPUStateSyncTable();
        } else {
            if(flag_interested_tb_prev == 0){
                runtime_env->addcpuStateSyncTable();
            } else {
                runtime_env->addEmptyCPUStateSyncTable();
            }
        }

        // 2.2 set the content of m_cpuState_post_insterest.second after each interested TB
        //    m_cpuState_post_insterest.second will only be used when the flag
        //    m_cpuState_post_insterest.first is set
        runtime_env->setCPUStatePostInterest(qemuCpuState);

        // 2.3 dump the current CPUState for cross checking on klee side
        runtime_env->addDebugCpuStateSyncTable(qemuCpuState);

        // 3. Memory Monitoring
        if(flag_interested_tb_prev == 0){
            runtime_env->addCurrentMemoSyncTable();
        } else {
            runtime_env->mergeCurrentMemoSyncTable();
        }

        // 4. Interrupt information:
        //    Add an empty interrupt state for each interested TB
        //    A real interrupt state will replace this empty state if
        //    an interrupt happened.
        runtime_env->addEmptyQemuInterruptState();

        // 5.
        is_post_interested_tb = 1;

        if(rt_dump_tb_count%CRETE_TRACING_WINDOW_SIZE == 0)
        {
            runtime_env->set_pending_stream();
        }
    }

    // Set runtime_env->m_cpuState_post_insterest
    if(flag_interested_tb_prev == 1 && flag_interested_tb == 0) {
        runtime_env->setFlagCPUStatePostInterest();
    }

    if(crete_tci_is_previous_block_symbolic())
    {
        runtime_env->addTBGraphExecSequ(tb.pc);
    }

    runtime_env->verifyDumpData();
    crete_tci_next_block();

#if defined(CRETE_DBG_CK)
    if(is_post_interested_tb) {
        fprintf(stderr, "1 - [POST] interested tb-%lu (pc-%p)\n",
                rt_dump_tb_count - 1, (void *)(uint64_t)rt_dump_tb->pc);
        if(is_in_list_crete_dbg_tb_pc(tb.pc)){
            CPUArchState* env = (CPUArchState *)qemuCpuState;
            fprintf(stderr, "crete_post_cpu_tb_exec():");

            {
                uint64_t i;
                const uint8_t *ptr_addr = (const uint8_t*)(&env->CRETE_DBG_REG);
                uint8_t byte_value;
                fprintf(stderr, "env->" CRETE_GET_STRING(CRETE_DBG_REG) " = [");
                for(i = 0; i < sizeof(env->CRETE_DBG_REG); ++i) {
                    byte_value = *(ptr_addr + i);
                    fprintf(stderr, " 0x%02x", byte_value);
                }
                fprintf(stderr, "]\n");
            }

            runtime_env->printDebugCpuStateSyncTable("debug_f");

            //        runtime_env->print();
            //        print_x86_all_cpu_regs((CPUArchState *)qemuCpuState);
        }
    } else {
        assert(reverse);

        if(!is_current_tb_executed)
            fprintf(stderr, "0 - [POST] reversing tb-%lu (pc-%p): TB not executed\n",
                                        rt_dump_tb_count, (void *)(uint64_t)rt_dump_tb->pc);
        else if (!crete_tci_is_current_block_symbolic())
            fprintf(stderr, "0 - [POST] reversing tb-%lu (pc-%p): TB not symbolic.\n",
                                        rt_dump_tb_count, (void *)(uint64_t)rt_dump_tb->pc);
        else if (tb.pc == crete_interrupted_pc)
            fprintf(stderr, "0 - [POST] reversing tb-%lu (pc-%p): TB being interrupted at the first instruction.\n",
                                        rt_dump_tb_count, (void *)(uint64_t)rt_dump_tb->pc);
        else
            assert(0);

    }
#endif

#if defined(CRETE_DBG_MEM)
    if(is_post_interested_tb)
        fprintf(stderr, "[CRETE_DBG_MEM] memory usage(tb-%lu) = %.3fMB\n",
                rt_dump_tb_count, crete_mem_usage());
#endif

    return is_post_interested_tb;
}

void add_qemu_interrupt_state(struct RuntimeEnv *rt,
        int intno, int is_int, int error_code, int next_eip_addend)
{
    QemuInterruptInfo interrup_info(intno, is_int, error_code, next_eip_addend);

    rt->addQemuInterruptState(interrup_info);
}

void dump_memo_sync_table_entry(struct RuntimeEnv *rt, uint64_t addr, uint32_t size, uint64_t value)
{
#if defined(CRETE_DBG_MEM_MONI)
    rt->addMemoSyncTableEntry(addr, size, value);
#endif

    rt->addCurrentMemoSyncTableEntry(addr, size, value);
}


void crete_set_capture_enabled(struct CreteFlags *cf, int capture_enabled)
{
	assert(cf);
	cf->set_capture_enabled((bool)capture_enabled);
}

int crete_flags_is_true(struct CreteFlags *cf)
{
	return cf->is_true();
}

#define __CRETE_CALC_CPU_SIDE_EFFECT(in_type, in_name)                              \
        if(memcmp(&reference->in_name, &target->in_name, sizeof(in_type)) != 0)     \
        {                                                                           \
            offset = CPU_OFFSET(in_name);                                           \
            size   = sizeof(in_type);                                               \
            name.str(string());                                                     \
            name << #in_name;                                                       \
            data.clear();                                                           \
                                                                                    \
            for(uint64_t j=0; j < size; ++j) {                                      \
                data.push_back(*(uint8_t *)(((uint8_t *)target)                     \
                        + offset + j));                                             \
            }                                                                       \
                                                                                    \
            ret.push_back(CPUStateElement(offset, size, name.str(), data));         \
        }


#define __CRETE_CALC_CPU_SIDE_EFFECT_ARRAY(in_type, in_name, array_size)            \
        for(uint64_t i = 0; i < (array_size); ++i)                                  \
        {                                                                           \
            if(memcmp(&reference->in_name[i], &target->in_name[i],                  \
                    sizeof(in_type)) != 0)                                          \
            {                                                                       \
                offset = CPU_OFFSET(in_name) + i*sizeof(in_type);                   \
                size   = sizeof(in_type);                                           \
                name.str(string());                                                 \
                name << #in_name << "[" << dec << i << "]";                         \
                data.clear();                                                       \
                                                                                    \
                for(uint64_t j=0; j < size; ++j) {                                  \
                    data.push_back(*(uint8_t *)(((uint8_t *)target)                 \
                            + offset + j));                                         \
                }                                                                   \
                                                                                    \
                ret.push_back(CPUStateElement(offset, size, name.str(), data));     \
            }                                                                       \
        }

// List of CPUState being ignored by cpuStateSyncTable
//  Element:                            Reason
// +--------------------------------+----------------------------+
// target_ulong eip;                  Irrelevant
// target_ulong cr[5];                Irrelevant + cross-check failure b/c irrelevant interrupt operations
// int error_code;                    Irrelevant + cross-check failure b/c irrelevant interrupt operations
// struct {} start_init_save;         Irrelevant + cannot trace
// struct {} end_init_save;           Irrelevant + cannot trace
// int old_exception;                 Irrelevant + cannot trace
// CPU_COMMON and all below           Irrelevant

// Compare two cpu states, return the different elements of target cpu state
static vector<CPUStateElement> x86_cpuState_compuate_side_effect(const CPUArchState *reference,
        const CPUArchState *target) {
    vector<CPUStateElement> ret;
    ret.clear();

    uint64_t offset;
    uint64_t size;
    stringstream name;
    vector<uint8_t> data;

    /* standard registers */
    // target_ulong regs[CPU_NB_REGS];
    __CRETE_CALC_CPU_SIDE_EFFECT_ARRAY(target_ulong, regs, CPU_NB_REGS)

//xxx: not traced
// target_ulong eip;

   // target_ulong eflags;
    __CRETE_CALC_CPU_SIDE_EFFECT(target_ulong, eflags)

    /* emulator internal eflags handling */
    // target_ulong cc_dst;
    __CRETE_CALC_CPU_SIDE_EFFECT(target_ulong, cc_dst)
    // target_ulong cc_src;
    __CRETE_CALC_CPU_SIDE_EFFECT(target_ulong, cc_src)
    // target_ulong cc_src2;
    __CRETE_CALC_CPU_SIDE_EFFECT(target_ulong, cc_src2)
    //uint32_t cc_op;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint32_t, cc_op);

    // int32_t df;
    __CRETE_CALC_CPU_SIDE_EFFECT(int32_t, df)
    // uint32_t hflags;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint32_t, hflags)
    // uint32_t hflags2;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint32_t, hflags2)

    /* segments */
    // SegmentCache segs[6];
    __CRETE_CALC_CPU_SIDE_EFFECT_ARRAY(SegmentCache, segs, 6)
    // SegmentCache ldt;
    __CRETE_CALC_CPU_SIDE_EFFECT(SegmentCache, ldt)
    // SegmentCache tr;
    __CRETE_CALC_CPU_SIDE_EFFECT(SegmentCache, tr)
    // SegmentCache gdt;
    __CRETE_CALC_CPU_SIDE_EFFECT(SegmentCache, gdt)
    // SegmentCache idt;
    __CRETE_CALC_CPU_SIDE_EFFECT(SegmentCache, idt)

// xxx: not traced
// target_ulong cr[5];
//    __CRETE_CALC_CPU_SIDE_EFFECT_ARRAY(target_ulong, cr, 5)

    // int32_t a20_mask;
    __CRETE_CALC_CPU_SIDE_EFFECT(int32_t, a20_mask)

    // BNDReg bnd_regs[4];
    __CRETE_CALC_CPU_SIDE_EFFECT_ARRAY(BNDReg, bnd_regs, 4)
    // BNDCSReg bndcs_regs;
    __CRETE_CALC_CPU_SIDE_EFFECT(BNDCSReg, bndcs_regs)
    // uint64_t msr_bndcfgs;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, msr_bndcfgs)

    /* Beginning of state preserved by INIT (dummy marker).  */
//xxx: not traced
//    struct {} start_init_save;
//    __CRETE_CALC_CPU_SIDE_EFFECT(struct {}, start_init_save)

    /* FPU state */
    // unsigned int fpstt;
    __CRETE_CALC_CPU_SIDE_EFFECT(unsigned int, fpstt)
    // uint16_t fpus;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint16_t, fpus)
    // uint16_t fpuc;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint16_t, fpuc)
    // uint8_t fptags[8];
    __CRETE_CALC_CPU_SIDE_EFFECT_ARRAY(uint8_t, fptags, 8)
    // FPReg fpregs[8];
    __CRETE_CALC_CPU_SIDE_EFFECT_ARRAY(FPReg, fpregs, 8)
    /* KVM-only so far */
    // uint16_t fpop;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint16_t, fpop)
    // uint64_t fpip;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, fpip)
    // uint64_t fpdp;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, fpdp)

    /* emulator internal variables */
    // float_status fp_status;
    __CRETE_CALC_CPU_SIDE_EFFECT(float_status, fp_status)
    // floatx80 ft0;
    __CRETE_CALC_CPU_SIDE_EFFECT(floatx80, ft0)

    // float_status mmx_status;
    __CRETE_CALC_CPU_SIDE_EFFECT(float_status, mmx_status)
    // float_status sse_status;
    __CRETE_CALC_CPU_SIDE_EFFECT(float_status, sse_status)
    // uint32_t mxcsr;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint32_t, mxcsr)
    // XMMReg xmm_regs[CPU_NB_REGS == 8 ? 8 : 32];
    __CRETE_CALC_CPU_SIDE_EFFECT_ARRAY(XMMReg, xmm_regs, CPU_NB_REGS == 8 ? 8 : 32)
    // XMMReg xmm_t0;
    __CRETE_CALC_CPU_SIDE_EFFECT(XMMReg, xmm_t0)
    // MMXReg mmx_t0;
    __CRETE_CALC_CPU_SIDE_EFFECT(MMXReg, mmx_t0)

    // uint64_t opmask_regs[NB_OPMASK_REGS];
    __CRETE_CALC_CPU_SIDE_EFFECT_ARRAY(uint64_t, opmask_regs, NB_OPMASK_REGS)

    /* sysenter registers */
    // uint32_t sysenter_cs;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint32_t, sysenter_cs)
    // target_ulong sysenter_esp;
    __CRETE_CALC_CPU_SIDE_EFFECT(target_ulong, sysenter_esp)
    // target_ulong sysenter_eip;
    __CRETE_CALC_CPU_SIDE_EFFECT(target_ulong, sysenter_eip)
    // uint64_t efer;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, efer)
    // uint64_t star;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, star)

    // uint64_t vm_hsave;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, vm_hsave)

#ifdef TARGET_X86_64
    // target_ulong lstar;
    __CRETE_CALC_CPU_SIDE_EFFECT(target_ulong, lstar)
    // target_ulong cstar;
    __CRETE_CALC_CPU_SIDE_EFFECT(target_ulong, cstar)
    // target_ulong fmask;
    __CRETE_CALC_CPU_SIDE_EFFECT(target_ulong, fmask)
    // target_ulong kernelgsbase;
    __CRETE_CALC_CPU_SIDE_EFFECT(target_ulong, kernelgsbase)
#endif

    // uint64_t tsc;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, tsc)
    // uint64_t tsc_adjust;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, tsc_adjust)
    // uint64_t tsc_deadline;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, tsc_deadline)

    // uint64_t mcg_status;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, mcg_status)
    // uint64_t msr_ia32_misc_enable;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, msr_ia32_misc_enable)
    // uint64_t msr_ia32_feature_control;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, msr_ia32_feature_control)

    // uint64_t msr_fixed_ctr_ctrl;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, msr_fixed_ctr_ctrl)
    // uint64_t msr_global_ctrl;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, msr_global_ctrl)
    // uint64_t msr_global_status;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, msr_global_status)
    // uint64_t msr_global_ovf_ctrl;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, msr_global_ovf_ctrl)
    // uint64_t msr_fixed_counters[MAX_FIXED_COUNTERS];
    __CRETE_CALC_CPU_SIDE_EFFECT_ARRAY(uint64_t, msr_fixed_counters, MAX_FIXED_COUNTERS)
    // uint64_t msr_gp_counters[MAX_GP_COUNTERS];
    __CRETE_CALC_CPU_SIDE_EFFECT_ARRAY(uint64_t, msr_gp_counters, MAX_GP_COUNTERS)
    // uint64_t msr_gp_evtsel[MAX_GP_COUNTERS];
    __CRETE_CALC_CPU_SIDE_EFFECT_ARRAY(uint64_t, msr_gp_evtsel, MAX_GP_COUNTERS)

    // uint64_t pat;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, pat)
    // uint32_t smbase;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint32_t, smbase)

    /* End of state preserved by INIT (dummy marker).  */
// xxx: not traced
//    struct {} end_init_save;
//    __CRETE_CALC_CPU_SIDE_EFFECT(struct {}, end_init_save)

    // uint64_t system_time_msr;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, system_time_msr)
    // uint64_t wall_clock_msr;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, wall_clock_msr)
    // uint64_t steal_time_msr;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, steal_time_msr)
    // uint64_t async_pf_en_msr;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, async_pf_en_msr)
    // uint64_t pv_eoi_en_msr;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, pv_eoi_en_msr)

    // uint64_t msr_hv_hypercall;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, msr_hv_hypercall)
    // uint64_t msr_hv_guest_os_id;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, msr_hv_guest_os_id)
    // uint64_t msr_hv_vapic;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, msr_hv_vapic)
    // uint64_t msr_hv_tsc;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, msr_hv_tsc)

    /* exception/interrupt handling */

// xxx: not traced
// int error_code;
//    __CRETE_CALC_CPU_SIDE_EFFECT(int, error_code)

    // int exception_is_int;
    __CRETE_CALC_CPU_SIDE_EFFECT(int, exception_is_int)
    // target_ulong exception_next_eip;
    __CRETE_CALC_CPU_SIDE_EFFECT(target_ulong, exception_next_eip)
    // target_ulong dr[8];
    __CRETE_CALC_CPU_SIDE_EFFECT_ARRAY(target_ulong, dr, 8)

//xxx: not traced
//    union {
//        struct CPUBreakpoint *cpu_breakpoint[4];
//        struct CPUWatchpoint *cpu_watchpoint[4];
//    };

    // int old_exception;
    __CRETE_CALC_CPU_SIDE_EFFECT(int, old_exception)
    // uint64_t vm_vmcb;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, vm_vmcb)
    // uint64_t tsc_offset;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, tsc_offset)
    // uint64_t intercept;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint64_t, intercept)
    // uint16_t intercept_cr_read;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint16_t, intercept_cr_read)
    // uint16_t intercept_cr_write;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint16_t, intercept_cr_write)
    // uint16_t intercept_dr_read;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint16_t, intercept_dr_read)
    // uint16_t intercept_dr_write;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint16_t, intercept_dr_write)
    // uint32_t intercept_exceptions;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint32_t, intercept_exceptions)
    // uint8_t v_tpr;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint8_t, v_tpr)

    /* KVM states, automatically cleared on reset */
    // uint8_t nmi_injected;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint8_t, nmi_injected)
    // uint8_t nmi_pending;
    __CRETE_CALC_CPU_SIDE_EFFECT(uint8_t, nmi_pending)

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
