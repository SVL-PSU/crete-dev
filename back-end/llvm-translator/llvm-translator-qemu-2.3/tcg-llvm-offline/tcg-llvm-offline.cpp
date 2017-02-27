#include "tcg-llvm-offline.h"

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include <string>
#include <stdlib.h>
#include <iostream>
#include <fstream>

#include "tcg.h"

#if defined(TCG_LLVM_OFFLINE)
#include "tcg-llvm.h"
#endif // defined(TCG_LLVM_OFFLINE)

using namespace std;

extern "C" {
#if !defined(TCG_LLVM_OFFLINE)
struct TCGHelperInfo;
extern const TCGHelperInfo all_helpers[];
extern TCGOpDef tcg_op_defs[];

uint64_t helpers_size = 0;
uint64_t opc_defs_size = 0;
#endif // !defined(TCG_LLVM_OFFLINE)
}

#if !defined(TCG_LLVM_OFFLINE)
void TCGLLVMOfflineContext::dump_tlo_tb_pc(const uint64_t pc)
{
    m_tlo_tb_pc.push_back(pc);
}

void TCGLLVMOfflineContext::dump_tcg_ctx(const TCGContext& tcg_ctx)
{
    m_tcg_ctx.push_back(tcg_ctx);
}

void TCGLLVMOfflineContext::dump_tcg_temp(const vector<TCGTemp>& tcg_temp)
{
    m_tcg_temps.push_back(tcg_temp);
}

void TCGLLVMOfflineContext::dump_tcg_helper_name(const TCGContext &tcg_ctx)
{
    for (uint64_t i = 0; i < helpers_size; ++i) {
        uint64_t helper_addr = (uint64_t)all_helpers[i].func;
        string helper_name(all_helpers[i].name);

        assert(helper_addr);
        assert(!helper_name.empty());

        m_helper_names.insert(make_pair(helper_addr, helper_name));
    }
}

void TCGLLVMOfflineContext::dump_tlo_tb_inst_count(const uint64_t inst_count)
{
    m_tlo_tb_inst_count.push_back(inst_count);
}

void TCGLLVMOfflineContext::dump_tbExecSequ(uint64_t pc, uint64_t unique_tb_num)
{
    m_tbExecSequ.push_back(make_pair(pc, unique_tb_num));
}

void TCGLLVMOfflineContext::dump_cpuState_size(uint64_t cpuState_size)
{
    m_cpuState_size = cpuState_size;
}
#endif // #if !defined(TCG_LLVM_OFFLINE)

uint64_t TCGLLVMOfflineContext::get_tlo_tb_pc(const uint64_t tb_index) const
{
    return m_tlo_tb_pc[tb_index];
}

const TCGContext& TCGLLVMOfflineContext::get_tcg_ctx(const uint64_t tb_index) const
{
    return m_tcg_ctx[tb_index];
}

const vector<TCGTemp>& TCGLLVMOfflineContext::get_tcg_temp(const uint64_t tb_index) const
{
    return m_tcg_temps[tb_index];
}

const map<uint64_t, string> TCGLLVMOfflineContext::get_helper_names() const
{
    return m_helper_names;
}

uint64_t TCGLLVMOfflineContext::get_tlo_tb_inst_count(const uint64_t tb_index) const
{
    return m_tlo_tb_inst_count[tb_index];
}

vector<pair<uint64_t, uint64_t> > TCGLLVMOfflineContext::get_tbExecSequ() const
{
    return m_tbExecSequ;
}

uint64_t TCGLLVMOfflineContext::get_cpuState_size() const
{
    return m_cpuState_size;
}

void TCGLLVMOfflineContext::print_info()
{
    cout  << dec << "m_tlo_tb_pc.size() = " << m_tlo_tb_pc.size() << endl
            << "m_helper_names.size() = " << m_helper_names.size() << endl
            << endl;

    cout  << dec << "sizeof (m_tlo_tb_pc) = " << sizeof(m_tlo_tb_pc)<< endl
            << "sizeof(m_helper_names) = " <<  sizeof(m_tcg_ctx)
            << "sizeof(m_tcg_temps) = " << sizeof(m_tcg_temps)
            << "sizeof(m_helper_names) = " << sizeof(m_helper_names) << endl
            << endl;


    cout << "pc values: ";
    uint64_t j = 0;
    for(vector<uint64_t>::iterator it = m_tlo_tb_pc.begin();
            it != m_tlo_tb_pc.end(); ++it) {
        cout << "tb-" << dec << j++ << ": pc = 0x" << hex << (*it) << endl;
    }


}

void TCGLLVMOfflineContext::dump_verify()
{
    assert(m_tlo_tb_pc.size() == m_tcg_ctx.size());
    assert(m_tlo_tb_pc.size() == m_tcg_temps.size());
}

uint64_t TCGLLVMOfflineContext::get_size()
{
    return (uint64_t)m_tlo_tb_pc.size();
}

#if defined(TCG_LLVM_OFFLINE)

extern "C" {
#include "config.h"
#include "exec-all.h"
}

#include <boost/filesystem.hpp>
#include <sstream>

#include <stdio.h>

#define CRETE_DEBUG

FILE *logfile;
int loglevel;

TCGContext tcg_ctx;

uint16_t gen_opc_buf[OPC_BUF_SIZE];
TCGArg gen_opparam_buf[OPPARAM_BUF_SIZE];

target_ulong gen_opc_pc[OPC_BUF_SIZE];
uint16_t gen_opc_icount[OPC_BUF_SIZE];
uint8_t gen_opc_instr_start[OPC_BUF_SIZE];

std::string crete_data_dir;

enum CreteFileType {
    CRETE_FILE_TYPE_LLVM_LIB,
    CRETE_FILE_TYPE_LLVM_TEMPLATE,
};

static void crete_set_data_dir(const char* data_dir)
{

    namespace fs = boost::filesystem;

    fs::path dpath(data_dir);

    if(!fs::exists(dpath))
    {
        throw std::runtime_error("failed to find data directory: " + dpath.string());
    }

    crete_data_dir = dpath.parent_path().string();

//  crete_data_dir = ".";
}

static std::string crete_find_file(CreteFileType type, const char *name)
{
    namespace fs = boost::filesystem;

    fs::path fpath = crete_data_dir;

    switch(type)
    {
    case CRETE_FILE_TYPE_LLVM_LIB:
//#if defined(TARGET_X86_64)
//        fpath /= "../x86_64-softmmu/";
//#else
//        fpath /= "../i386-softmmu/";
//#endif // defined(TARGET_X86_64)
        break;
    case CRETE_FILE_TYPE_LLVM_TEMPLATE:
        // fpath /= "../runtime-dump/";
        break;
    }

    fpath /= name;

    if(!fs::exists(fpath))
    {
        throw std::runtime_error("failed to find file: " + fpath.string());
    }

    return fpath.string();
}

static void dump_tcg_op_defs()
{
    char file_name[] = "translator-op-def.txt";
    FILE *f = fopen(file_name, "w");
    assert(f);

    for(uint32_t i = 0; i < tcg_op_defs_max; ++i) {
        fprintf(f, "opc = %d, name = %s\n", i, tcg_op_defs[i].name);
    }

    fclose(f);
}

const char * get_helper_name(uint64_t func_addr)
{
    assert(tcg_llvm_ctx);
    string func_name = tcg_llvm_ctx->get_crete_helper_name(func_addr);
    char * ret = (char *) malloc(func_name.length() + 1);
    strcpy(ret, func_name.c_str());

    cerr << "func_name = " << ret
            << ", func_addr = 0x" << hex << func_addr
            << ", ret = 0x" << (uint64_t) ret << endl;

    return ret;
}

void x86_llvm_translator()
{
    namespace fs = boost::filesystem;

#if defined(CRETE_DEBUG)
    cerr<< "this is the new main function from tcg-llvm-offline.\n" << endl;

    cerr<< "sizeof(TCGContext_temp) = 0x" << hex << sizeof(TCGContext_temp)
                << "sizeof(TCGArg) = 0x" << sizeof(TCGArg) << endl
                << ", OPPARAM_BUF_SIZE = 0x" << OPPARAM_BUF_SIZE
                << ", OPC_BUF_SIZE = 0x" << OPC_BUF_SIZE
                << ", MAX_OPC_PARAM = 0x" << MAX_OPC_PARAM << endl;

    dump_tcg_op_defs();
#endif

    //1. initialize llvm dependencies
    tcg_llvm_ctx = tcg_llvm_initialize();
    assert(tcg_llvm_ctx);

#if defined(TARGET_X86_64)
    tcg_linkWithLibrary(tcg_llvm_ctx,
            crete_find_file(CRETE_FILE_TYPE_LLVM_LIB, "crete-qemu-2.3-op-helper-x86_64.bc").c_str());
#elif defined(TARGET_I386)
    tcg_linkWithLibrary(tcg_llvm_ctx,
            crete_find_file(CRETE_FILE_TYPE_LLVM_LIB, "crete-qemu-2.3-op-helper-i386.bc").c_str());
#else
    #error CRETE: Only I386 and x64 supported!
#endif // defined(TARGET_X86_64) || defined(TARGET_I386)



    stringstream ss;
    uint64_t streamed_count = 0;
    for(;;) {
        ss.str(string());
        ss << "dump_tcg_llvm_offline." << streamed_count++ << ".bin";
        if(!fs::exists(ss.str())){
            cerr << ss.str() << " not found\n";
            break;
        }

        cerr << ss.str() << " being found\n";

        //2. initialize tcg_llvm_ctx_offline
        TCGLLVMOfflineContext temp_tcg_llvm_offline_ctx;

        ifstream ifs(ss.str().c_str());
        boost::archive::binary_iarchive ia(ifs);
        ia >> temp_tcg_llvm_offline_ctx;
        temp_tcg_llvm_offline_ctx.dump_verify();

#if defined(CRETE_DEBUG)
        temp_tcg_llvm_offline_ctx.print_info();
#endif

        if(streamed_count == 1){
            tcg_llvm_ctx->crete_init_helper_names(temp_tcg_llvm_offline_ctx.get_helper_names());
            tcg_llvm_ctx->crete_set_cpuState_size(temp_tcg_llvm_offline_ctx.get_cpuState_size());
        }

        tcg_llvm_ctx->crete_add_tbExecSequ(temp_tcg_llvm_offline_ctx.get_tbExecSequ());

        //3. Translate
        TranslationBlock temp_tb = {};
        TCGContext *s = &tcg_ctx;

        for(uint64_t i = 0; i < temp_tcg_llvm_offline_ctx.get_size(); ++i) {
            //3.1 update temp_tb
            temp_tb.pc = (target_long)temp_tcg_llvm_offline_ctx.get_tlo_tb_pc(i);

            //3.2 update tcg_ctx
            const TCGContext temp_tcg_ctx = temp_tcg_llvm_offline_ctx.get_tcg_ctx(i);
            memcpy((void *)s, (void *)&temp_tcg_ctx, sizeof(TCGContext));

            //3.3 update gen_opc_buf and gen_opparam_buf

            for(uint64_t j = 0; j < OPC_BUF_SIZE; ++j) {
                gen_opc_buf[j] = (uint16_t)temp_tcg_ctx.gen_op_buf[j].opc;
            }

            for(uint64_t j = 0; j < OPPARAM_BUF_SIZE; ++j) {
                gen_opparam_buf[j] = (TCGArg)temp_tcg_ctx.gen_opparam_buf[j];
            }

            // 3.4 update tcg-temp
            const vector<TCGTemp> temp_tcg_temp = temp_tcg_llvm_offline_ctx.get_tcg_temp(i);
            assert( temp_tcg_temp.size() == s->nb_temps);
            for(uint64_t j = 0; j < s->nb_temps; ++j)
                s->temps[j].assign(temp_tcg_temp[j]);

            // generate offline-tbir.txt
            uint64_t tb_inst_count = temp_tcg_llvm_offline_ctx.get_tlo_tb_inst_count(i);

            static unsigned long long  tbir_count = 0;
            char file_name[] = "offline-tbir.txt";
            FILE *f = fopen(file_name, "a");
            assert(f);

            fprintf(f, "qemu-ir-tb-%llu-%llu: tb_inst_count = %llu\n",
                    tbir_count++, temp_tb.pc, tb_inst_count);
            tcg_dump_ops_file(s, f);
            fprintf(f, "\n");

            fclose(f);

            //3.5 generate llvm bitcode

            cerr << "tcg_llvm_ctx->generateCode(s, &temp_tb) will be invoked." << endl;
            temp_tb.tcg_llvm_context = NULL;
            temp_tb.llvm_function = NULL;

            tcg_llvm_ctx->generateCode(s, &temp_tb);

            cerr<< "tcg_llvm_ctx->generateCode(s, &temp_tb) is done." << endl;

            assert(temp_tb.tcg_llvm_context != NULL);
            assert(temp_tb.llvm_function != NULL);
        }
    }

    //4. generate main function
    tcg_llvm_ctx->generate_crete_main();

    //5. Write out the translated llvm bitcode to file in the current folder
    fs::path bitcode_path = fs::current_path() / "dump_llvm_offline.bc";
    tcg_llvm_ctx->writeBitCodeToFile(bitcode_path.string());

    cerr << "offline translator is done.\n" << endl;
    //6. cleanup
    //    delete tcg_llvm_offline_ctx;
}

int main(int argc, char **argv) {
    crete_set_data_dir(argv[0]);
    x86_llvm_translator();

    return 0;
}
#endif // defined(TCG_LLVM_OFFLINE)
