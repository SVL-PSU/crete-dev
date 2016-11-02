/*
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

#ifndef TCG_LLVM_H
#define TCG_LLVM_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

//#include "tcg.h"

/*****************************/
/* Functions for QEMU c code */

struct TranslationBlock;
struct TCGLLVMContext;

extern struct TCGLLVMContext* tcg_llvm_ctx;

struct TCGLLVMContext* tcg_llvm_initialize(void);
void tcg_llvm_close(struct TCGLLVMContext *l);

void tcg_llvm_tb_alloc(struct TranslationBlock *tb);
void tcg_llvm_tb_free(struct TranslationBlock *tb);

void tcg_llvm_gen_code(struct TCGLLVMContext *l, struct TCGContext *s,
                       struct TranslationBlock *tb);

const char* tcg_llvm_get_func_name(struct TranslationBlock *tb);

void tcg_llvm_initHelper(struct TCGLLVMContext *l);

#if defined(TCG_LLVM_OFFLINE)
void cpu_gen_llvm(CPUState *env, TranslationBlock *tb);
int get_llvm_tbCount(struct TCGLLVMContext *l);
void tcg_linkWithLibrary(struct TCGLLVMContext *l, const char *libraryName);
#else
#error
#endif

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

/***********************************/
/* External interface for C++ code */
#include <string>
#include <map>
#include <vector>

using namespace std;
namespace llvm {
    class Function;
    class LLVMContext;
    class Module;
    class ModuleProvider;
    class ExecutionEngine;
    class FunctionPassManager;
}

class TCGLLVMContextPrivate;
class TCGLLVMContext
{
private:
    TCGLLVMContextPrivate* m_private;

public:
    TCGLLVMContext();
    ~TCGLLVMContext();

    llvm::LLVMContext& getLLVMContext();

    llvm::Module* getModule();
    llvm::ModuleProvider* getModuleProvider();

    llvm::ExecutionEngine* getExecutionEngine();

    void deleteExecutionEngine();
    llvm::FunctionPassManager* getFunctionPassManager() const;

    /** Called after linking all helper libraries */
    void initializeHelpers();

    void generateCode(struct TCGContext *s,
                      struct TranslationBlock *tb);

#ifdef TCG_LLVM_OFFLINE
    int getTbCount();
    void writeBitCodeToFile(const std::string &fileName);
    void linkWithLibrary(const std::string& libraryName);

    void crete_init_helper_names(const map<uint64_t, string>& helper_names);
    const string get_crete_helper_name(const uint64_t func_addr) const;

    void crete_set_cpuState_size(uint64_t cpuState_size);
    void crete_add_tbExecSequ(vector<pair<uint64_t, uint64_t> > seq);

    void generate_crete_main();
#else
#error "ERROR"
#endif

};

#endif

#endif
