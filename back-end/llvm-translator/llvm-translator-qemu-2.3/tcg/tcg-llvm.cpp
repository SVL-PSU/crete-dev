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

#include <boost/serialization/split_member.hpp>
#include <string>
#include <stdlib.h>

extern "C" {
#include "tcg.h"
}

#include "tcg-llvm.h"

extern "C" {
//#include "config.h"
#include "qemu-common.h"
#include "disas.h"

#if defined(CONFIG_SOFTMMU)

#include "softmmu_defs.h"


static void *qemu_ld_helpers[5] = {
    (void*) __ldb_mmu,
    (void*) __ldw_mmu,
    (void*) __ldl_mmu,
    (void*) __ldq_mmu,
    (void*) __ldq_mmu,
};

static void *qemu_st_helpers[5] = {
    (void*) __stb_mmu,
    (void*) __stw_mmu,
    (void*) __stl_mmu,
    (void*) __stq_mmu,
    (void*) __stq_mmu,
};

#endif

int execute_llvm = 0;
}

#include <llvm/DerivedTypes.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JITMemoryManager.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/PassManager.h>
#include <llvm/Intrinsics.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/DataLayout.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/IRBuilder.h>
#include <llvm/Support/Threading.h>

#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/raw_ostream.h>

#if defined(TCG_LLVM_OFFLINE)
#include <llvm/Bitcode/ReaderWriter.h>
#include "llvm/Linker.h"
#include "llvm/Support/Path.h"

#include "tcg-llvm-offline/tcg-llvm-offline.h"
#endif // #if defined(TCG_LLVM_OFFLINE)

#include <iostream>
#include <sstream>

//#undef NDEBUG

extern "C" {
    TCGLLVMContext* tcg_llvm_ctx = 0;
}

// map from label_ptr to label_idx, assumption here is the label pointer will
// always point to a and only one label struct object
static std::map<uint64_t, uint64_t> map_label;
static uint64_t last_idx = 0;

using namespace llvm;

class TJITMemoryManager;

struct TCGLLVMContextPrivate {
    LLVMContext& m_context;
    IRBuilder<> m_builder;

    /* Current m_module */
    Module *m_module;

    /* JIT engine */
    TJITMemoryManager *m_jitMemoryManager;
    ExecutionEngine *m_executionEngine;

    /* Function pass manager (used for optimizing the code) */
    FunctionPassManager *m_functionPassManager;

    /* Declaration of a wrapper function for helpers */
    Function *m_helperTraceMemoryAccess;
    Function *m_helperTraceInstruction;
    Function *m_helperForkAndConcretize;
    Function *m_helperMakeSymbolic;
    Function *m_helperGetValue;
    Function* m_qemu_ld_helpers[5];
    Function* m_qemu_st_helpers[5];

    // BOBO: xxx potential optimization
    Function** new_qemu_memo_helpers;

    /* Count of generated translation blocks */
    int m_tbCount;

    /* XXX: The following members are "local" to generateCode method */

    /* TCGContext for current translation block */
    TCGContext* m_tcgContext;

    /* Function for current translation block */
    Function *m_tbFunction;

    /* Current temp m_values */
    Value* m_values[TCG_MAX_TEMPS];

    /* Pointers to in-memory versions of globals or local temps */
    Value* m_memValuesPtr[TCG_MAX_TEMPS];

    /* For reg-based globals, store argument number,
     * for mem-based globals, store base value index */
    int m_globalsIdx[TCG_MAX_TEMPS];

    BasicBlock* m_labels[TCG_MAX_LABELS];

public:
    TCGLLVMContextPrivate();
    ~TCGLLVMContextPrivate();

#if defined(TCG_LLVM_OFFLINE)
    void writeBitCodeToFile(const std::string& fileName) {
        verifyModule(*m_module, PrintMessageAction);

        std::string error;
        llvm::raw_fd_ostream o(fileName.c_str(), error, llvm::raw_fd_ostream::F_Binary);

        // Output the bitcode file to stdout
        llvm::WriteBitcodeToFile(m_module, o);
    }
#endif

    void deleteExecutionEngine() {
        if (m_executionEngine) {
            delete m_executionEngine;
            m_executionEngine = NULL;
        }
    }

    FunctionPassManager *getFunctionPassManager() const {
        return m_functionPassManager;
    }

    /* Shortcuts */
    llvm::Type* intType(int w) { return IntegerType::get(m_context, w); }
    llvm::Type* intPtrType(int w) { return PointerType::get(intType(w), 0); }
    llvm::Type* wordType() { return intType(TCG_TARGET_REG_BITS); }
    llvm::Type* wordType(int bits) { return intType(bits); }
    llvm::Type* wordPtrType() { return intPtrType(TCG_TARGET_REG_BITS); }

    void adjustTypeSize(unsigned target, Value **v1) {
        Value *va = *v1;
        if (target == 32) {
            if (va->getType() == intType(64)) {
                *v1 = m_builder.CreateTrunc(va, intType(target));
            } else if (va->getType() != intType(32)) {
                assert(false);
            }
        }
    }

    //This handles the special case of symbolic values
    //assigned to the program counter register
    Value* handleSymbolicPcAssignment(Value *orig) {
        return orig;
    }

    void adjustTypeSize(unsigned target, Value **v1, Value **v2) {
        adjustTypeSize(target, v1);
        adjustTypeSize(target, v2);
    }

    llvm::Type* tcgType(int type) {
        return type == TCG_TYPE_I64 ? intType(64) : intType(32);
    }

    llvm::Type* tcgPtrType(int type) {
        return type == TCG_TYPE_I64 ? intPtrType(64) : intPtrType(32);
    }

    /* Helpers */
    Value* getValue(int idx);
    void setValue(int idx, Value *v);
    void delValue(int idx);

    Value* getPtrForValue(int idx);
    void delPtrForValue(int idx);
    void initGlobalsAndLocalTemps();
    unsigned getValueBits(int idx);

    void invalidateCachedMemory();

    uint64_t toInteger(Value *v) const {
        if (ConstantInt *cste = dyn_cast<ConstantInt>(v)) {
            return *cste->getValue().getRawData();
        }
        llvm::errs() << *v << '\n';
        assert(false && "Not a constant");
    }

    void initializeHelpers();


    BasicBlock* getLabel(uint64_t idx);
    void clearLabels();
    void delLabel(int idx);
    void startNewBasicBlock(BasicBlock *bb = NULL);

    /* Code generation */
    Value* generateQemuMemOp(bool ld, Value *value, Value *addr,
                             int mem_index, int bits);

    void generateTraceCall(uintptr_t pc);
    int generateOperation(TCGOp* op, int opc, const TCGArg *args);

    void generateCode(TCGContext *s, TranslationBlock *tb);

    Value* new_generateQemuMemOp(bool ld, Value *value,
            Value *addr, TCGArg memop, int mem_index, int bits);
    Value* getLdMOValue(Value *value, TCGArg memop, int bits);

    void crete_init_helper_names(const map<uint64_t, string>& helper_names);
    const string get_crete_helper_name(const uint64_t func_addr) const;

    void crete_set_cpuState_size(uint64_t cpuState_size);
    void crete_add_tbExecSequ(vector<pair<uint64_t, uint64_t> > seq);

    void generate_crete_main();

private:
    map<uint64_t, string> m_crete_helper_names;

    // Execution sequence of cpatured TB: <pc, unique-tb-number>
    vector<pair<uint64_t, uint64_t> > m_tbExecSequ;
    uint64_t m_cpuState_size;
};

/* Custom JITMemoryManager in order to capture the size of
 * the last generated function */
class TJITMemoryManager: public JITMemoryManager {
    JITMemoryManager* m_base;
    ptrdiff_t m_lastFunctionSize;
public:
    TJITMemoryManager():
        m_base(JITMemoryManager::CreateDefaultMemManager()),
        m_lastFunctionSize(0) {}
    ~TJITMemoryManager() { delete m_base; }

    ptrdiff_t getLastFunctionSize() const { return m_lastFunctionSize; }

    uint8_t *startFunctionBody(const Function *F, uintptr_t &ActualSize) {
        m_lastFunctionSize = 0;
        return m_base->startFunctionBody(F, ActualSize);
    }
    void endFunctionBody(const Function *F, uint8_t *FunctionStart,
                                uint8_t *FunctionEnd) {
        m_lastFunctionSize = FunctionEnd - FunctionStart;
        m_base->endFunctionBody(F, FunctionStart, FunctionEnd);
    }

    void setMemoryWritable() { m_base->setMemoryWritable(); }
    void setMemoryExecutable() { m_base->setMemoryExecutable(); }
    void setPoisonMemory(bool poison) { m_base->setPoisonMemory(poison); }
    void AllocateGOT() { m_base->AllocateGOT(); }
    uint8_t *getGOTBase() const { return m_base->getGOTBase(); }
    //void SetDlsymTable(void *ptr) { m_base->SetDlsymTable(ptr); }
    //void *getDlsymTable() const { return m_base->getDlsymTable(); }
    uint8_t *allocateStub(const GlobalValue* F, unsigned StubSize,
                                unsigned Alignment) {
        return m_base->allocateStub(F, StubSize, Alignment);
    }
    uint8_t *allocateSpace(intptr_t Size, unsigned Alignment) {
        return m_base->allocateSpace(Size, Alignment);
    }
    uint8_t *allocateGlobal(uintptr_t Size, unsigned Alignment) {
        return m_base->allocateGlobal(Size, Alignment);
    }
    uint8_t *allocateCodeSection(uintptr_t Size, unsigned Alignment,
                                 unsigned SectionID) {
        return m_base->allocateCodeSection(Size, Alignment, SectionID);
    }
    uint8_t *allocateDataSection(uintptr_t Size, unsigned Alignment,
                                 unsigned SectionID) {
        return m_base->allocateDataSection(Size, Alignment, SectionID);
    }
    void *getPointerToNamedFunction(const std::string &Name,
                                    bool AbortOnFailure = true) {
        return m_base->getPointerToNamedFunction(Name, AbortOnFailure);
    }
    //void deallocateMemForFunction(const Function *F) {
    //    m_base->deallocateMemForFunction(F);
    //}

    virtual void deallocateFunctionBody(void *Body) {
        m_base->deallocateFunctionBody(Body);
    }

    uint8_t* startExceptionTable(const Function* F, uintptr_t &ActualSize) {
        return m_base->startExceptionTable(F, ActualSize);
    }
    void endExceptionTable(const Function *F, uint8_t *TableStart,
                                 uint8_t *TableEnd, uint8_t* FrameRegister) {
        m_base->endExceptionTable(F, TableStart, TableEnd, FrameRegister);
    }
    virtual void deallocateExceptionTable(void *Body) {
        m_base->deallocateExceptionTable(Body);
    }
    bool CheckInvariants(std::string &ErrorStr) {
        return m_base->CheckInvariants(ErrorStr);
    }
    size_t GetDefaultCodeSlabSize() {
        return m_base->GetDefaultCodeSlabSize();
    }
    size_t GetDefaultDataSlabSize() {
        return m_base->GetDefaultDataSlabSize();
    }
    size_t GetDefaultStubSlabSize() {
        return m_base->GetDefaultStubSlabSize();
    }
    unsigned GetNumCodeSlabs() { return m_base->GetNumCodeSlabs(); }
    unsigned GetNumDataSlabs() { return m_base->GetNumDataSlabs(); }
    unsigned GetNumStubSlabs() { return m_base->GetNumStubSlabs(); }
};

TCGLLVMContextPrivate::TCGLLVMContextPrivate()
    : m_context(getGlobalContext()), m_builder(m_context), m_tbCount(0),
      m_tcgContext(NULL), m_tbFunction(NULL)
{
    std::memset(m_values, 0, sizeof(m_values));
    std::memset(m_memValuesPtr, 0, sizeof(m_memValuesPtr));
    std::memset(m_globalsIdx, 0, sizeof(m_globalsIdx));
    std::memset(m_labels, 0, sizeof(m_labels));

    InitializeNativeTarget();

    m_module = new Module("tcg-llvm", m_context);

    m_jitMemoryManager = new TJITMemoryManager();

    std::string error;
    m_executionEngine = ExecutionEngine::createJIT(
            m_module, &error, m_jitMemoryManager);
    if(m_executionEngine == NULL) {
        std::cerr << "Unable to create LLVM JIT: " << error << std::endl;
        exit(1);
    }

    m_functionPassManager = new FunctionPassManager(m_module);
    m_functionPassManager->add(
            new DataLayout(*m_executionEngine->getDataLayout()));

    m_functionPassManager->add(createReassociatePass());
    m_functionPassManager->add(createConstantPropagationPass());
    m_functionPassManager->add(createInstructionCombiningPass());
    m_functionPassManager->add(createGVNPass());
    m_functionPassManager->add(createDeadStoreEliminationPass());
    m_functionPassManager->add(createCFGSimplificationPass());
    m_functionPassManager->add(createPromoteMemoryToRegisterPass());

    //m_functionPassManager->add(new SelectRemovalPass());

    m_functionPassManager->doInitialization();
}

TCGLLVMContextPrivate::~TCGLLVMContextPrivate()
{
    delete m_functionPassManager;

    // the following line will also delete
    // m_moduleProvider, m_module and all its functions
    if (m_executionEngine) {
        delete m_executionEngine;
    }
}


void TCGLLVMContextPrivate::initializeHelpers()
{
    m_qemu_ld_helpers[0] = m_module->getFunction("__ldb_mmu");
    m_qemu_ld_helpers[1] = m_module->getFunction("__ldw_mmu");
    m_qemu_ld_helpers[2] = m_module->getFunction("__ldl_mmu");
    m_qemu_ld_helpers[3] = m_module->getFunction("__ldq_mmu");
    m_qemu_ld_helpers[4] = m_module->getFunction("__ldq_mmu");

    m_qemu_st_helpers[0] = m_module->getFunction("__stb_mmu");
    m_qemu_st_helpers[1] = m_module->getFunction("__stw_mmu");
    m_qemu_st_helpers[2] = m_module->getFunction("__stl_mmu");
    m_qemu_st_helpers[3] = m_module->getFunction("__stq_mmu");
    m_qemu_st_helpers[4] = m_module->getFunction("__stq_mmu");

//    assert(m_helperTraceMemoryAccess);
// XXX: is this really not needed on ARM?
#ifndef TARGET_ARM
//    assert(m_helperMakeSymbolic);
#endif
//    assert(m_helperGetValue);
    for(int i = 0; i < 5; ++i) {
        assert(m_qemu_ld_helpers[i]);
        assert(m_qemu_st_helpers[i]);
    }
}

Value* TCGLLVMContextPrivate::getPtrForValue(int idx)
{
    TCGContext *s = m_tcgContext;
    TCGTemp &temp = s->temps[idx];

    assert(idx < s->nb_globals || s->temps[idx].temp_local);

    if(m_memValuesPtr[idx] == NULL) {
        assert(idx < s->nb_globals);

        if(temp.fixed_reg) {
            Value *v = m_builder.CreateConstGEP1_32(
                    m_tbFunction->arg_begin(), m_globalsIdx[idx]);
            m_memValuesPtr[idx] = m_builder.CreatePointerCast(
                    v, tcgPtrType(temp.type)
#ifndef NDEBUG
                    , StringRef(temp.name) + "_ptr"
#endif
                    );

        } else {
            Value *v = getValue(m_globalsIdx[idx]);
            assert(v->getType() == wordType());

            v = m_builder.CreateAdd(v, ConstantInt::get(
                            wordType(), temp.mem_offset));
            m_memValuesPtr[idx] =
                m_builder.CreateIntToPtr(v, tcgPtrType(temp.type)
#ifndef NDEBUG
                        , StringRef(temp.name) + "_ptr"
#endif
                        );
        }
    }

    return m_memValuesPtr[idx];
}

inline void TCGLLVMContextPrivate::delValue(int idx)
{
    /* XXX
    if(m_values[idx] && m_values[idx]->use_empty()) {
        if(!isa<Instruction>(m_values[idx]) ||
                !cast<Instruction>(m_values[idx])->getParent())
            delete m_values[idx];
    }
    */
    m_values[idx] = NULL;
}

inline void TCGLLVMContextPrivate::delPtrForValue(int idx)
{
    /* XXX
    if(m_memValuesPtr[idx] && m_memValuesPtr[idx]->use_empty()) {
        if(!isa<Instruction>(m_memValuesPtr[idx]) ||
                !cast<Instruction>(m_memValuesPtr[idx])->getParent())
            delete m_memValuesPtr[idx];
    }
    */
    m_memValuesPtr[idx] = NULL;
}

unsigned TCGLLVMContextPrivate::getValueBits(int idx)
{
    switch (m_tcgContext->temps[idx].type) {
        case TCG_TYPE_I32: return 32;
        case TCG_TYPE_I64: return 64;
        default: assert(false && "Unknown size");
    }
    return 0;
}

Value* TCGLLVMContextPrivate::getValue(int idx)
{
    if(m_values[idx] == NULL) {
        if(idx < m_tcgContext->nb_globals) {
            m_values[idx] = m_builder.CreateLoad(getPtrForValue(idx)
#ifndef NDEBUG
                    , StringRef(m_tcgContext->temps[idx].name) + "_v"
#endif
                    );
        } else if(m_tcgContext->temps[idx].temp_local) {
            m_values[idx] = m_builder.CreateLoad(getPtrForValue(idx));
#ifndef NDEBUG
            std::ostringstream name;
            name << "loc" << (idx - m_tcgContext->nb_globals) << "_v";
            m_values[idx]->setName(name.str());
#endif
        } else {
            // Temp value was not previousely assigned
            assert(false); // XXX: or return zero constant ?
        }
    }

    // std::cerr << "getValue(): idx = " << std::dec << idx
    //         << "with: " << m_values[idx]->getType()->getIntegerBitWidth()<< std::endl;

    return m_values[idx];
}

void TCGLLVMContextPrivate::setValue(int idx, Value *v)
{
    // std::cerr << "setValue(): idx = " << std::dec << idx << std::endl;

    delValue(idx);
    m_values[idx] = v;

    if(!v->hasName() && !isa<Constant>(v)) {
#ifndef NDEBUG
        if(idx < m_tcgContext->nb_globals)
            v->setName(StringRef(m_tcgContext->temps[idx].name) + "_v");
        if(m_tcgContext->temps[idx].temp_local) {
            std::ostringstream name;
            name << "loc" << (idx - m_tcgContext->nb_globals) << "_v";
            v->setName(name.str());
        } else {
            std::ostringstream name;
            name << "tmp" << (idx - m_tcgContext->nb_globals) << "_v";
            v->setName(name.str());
        }
#endif
    }

    if(idx < m_tcgContext->nb_globals) {
        // We need to save a global copy of a value
        m_builder.CreateStore(v, getPtrForValue(idx));

        if(m_tcgContext->temps[idx].fixed_reg) {
            /* Invalidate all dependent global vals and pointers */
            for(int i=0; i<m_tcgContext->nb_globals; ++i) {
                if(i != idx && !m_tcgContext->temps[idx].fixed_reg &&
                                    m_globalsIdx[i] == idx) {
                    delValue(i);
                    delPtrForValue(i);
                }
            }
        }
    } else if(m_tcgContext->temps[idx].temp_local) {
        // We need to save an in-memory copy of a value
        m_builder.CreateStore(v, getPtrForValue(idx));
    }
}

void TCGLLVMContextPrivate::initGlobalsAndLocalTemps()
{
    TCGContext *s = m_tcgContext;

    int reg_to_idx[TCG_TARGET_NB_REGS];
    for(int i=0; i<TCG_TARGET_NB_REGS; ++i)
        reg_to_idx[i] = -1;

    int argNumber = 0;
    for(int i=0; i<s->nb_globals; ++i) {
        if(s->temps[i].fixed_reg) {
            // This global is in fixed host register. We are
            // mapping such registers to function arguments
            m_globalsIdx[i] = argNumber++;
            reg_to_idx[s->temps[i].reg] = i;

        } else {
            // This global is in memory at (mem_reg + mem_offset).
            // Base value is not known yet, so just store mem_reg
            m_globalsIdx[i] = s->temps[i].mem_reg;
        }
    }

    // Map mem_reg to index for memory-based globals
    for(int i=0; i<s->nb_globals; ++i) {
        if(!s->temps[i].fixed_reg) {
            assert(reg_to_idx[m_globalsIdx[i]] >= 0);
            m_globalsIdx[i] = reg_to_idx[m_globalsIdx[i]];
        }
    }

    // Allocate local temps
    for(int i=s->nb_globals; i<TCG_MAX_TEMPS; ++i) {
        if(s->temps[i].temp_local) {
            std::ostringstream pName;
            pName << "loc_" << (i - s->nb_globals) << "ptr";
            m_memValuesPtr[i] = m_builder.CreateAlloca(
                tcgType(s->temps[i].type), 0, pName.str());
        }
    }
}

inline BasicBlock* TCGLLVMContextPrivate::getLabel(uint64_t label_ptr)
{
    // std::cerr << "label_ptr = 0x" << std::hex << label_ptr << ", TCG_MAX_LABELS = 0x" << TCG_MAX_LABELS << std::endl;

    uint64_t idx;

    std::map<uint64_t, uint64_t>::iterator it = map_label.find(label_ptr);
    if(it  == map_label.end() )
    {
        idx = last_idx++;
        assert(!m_labels[idx]);
        assert(idx < TCG_MAX_LABELS);

        map_label.insert(std::make_pair(label_ptr, idx));

        std::ostringstream bbName;
        bbName << "label_" << idx;
        m_labels[idx] = BasicBlock::Create(m_context, bbName.str());
    } else {
        idx = it->second;
        assert(m_labels[idx]);
    }

    return m_labels[idx];

//    if(!m_labels[idx]) {
//        std::ostringstream bbName;
//        bbName << "label_" << idx;
//        m_labels[idx] = BasicBlock::Create(m_context, bbName.str());
//    }
//    return m_labels[idx];
}

void TCGLLVMContextPrivate::clearLabels() {
    map_label.clear();
    last_idx = 0;
    std::memset(m_labels, 0, sizeof(m_labels));
}

inline void TCGLLVMContextPrivate::delLabel(int idx)
{
    /* XXX
    if(m_labels[idx] && m_labels[idx]->use_empty() &&
            !m_labels[idx]->getParent())
        delete m_labels[idx];
    */
    m_labels[idx] = NULL;
}

void TCGLLVMContextPrivate::startNewBasicBlock(BasicBlock *bb)
{
    if(!bb)
        bb = BasicBlock::Create(m_context);
    else
        assert(bb->getParent() == 0);

    if(!m_builder.GetInsertBlock()->getTerminator())
        m_builder.CreateBr(bb);

    m_tbFunction->getBasicBlockList().push_back(bb);
    m_builder.SetInsertPoint(bb);

    /* Invalidate all temps */
    for(int i=0; i<TCG_MAX_TEMPS; ++i)
        delValue(i);

    /* Invalidate all pointers to globals */
    for(int i=0; i<m_tcgContext->nb_globals; ++i)
        delPtrForValue(i);
}

inline Value* TCGLLVMContextPrivate::generateQemuMemOp(bool ld,
        Value *value, Value *addr, int mem_index, int bits)
{
    assert(addr->getType() == intType(TARGET_LONG_BITS));
    assert(ld || value->getType() == intType(bits));
    assert(TCG_TARGET_REG_BITS == 64); //XXX

#ifdef CONFIG_SOFTMMU
    assert(!execute_llvm);

    if(ld) {
        return m_builder.CreateCall2(m_qemu_ld_helpers[bits>>4], addr,
                    ConstantInt::get(intType(8*sizeof(int)), mem_index));
    } else {

#if defined(CRETE_DEBUG) && defined(CRETE_DEBUG_LLVM)
    m_qemu_st_helpers[bits>>4]->dump();
    addr->dump();
    value->dump();
    ConstantInt::get(intType(8*sizeof(int)), mem_index)->dump();
#endif // defined(CRETE_DEBUG) && defined(CRETE_DEBUG_LLVM)

        m_builder.CreateCall3(m_qemu_st_helpers[bits>>4], addr, value,
                    ConstantInt::get(intType(8*sizeof(int)), mem_index));
        return NULL;
    }

#else // CONFIG_SOFTMMU
    assert(0); // for llvm-offline-translator, it should always be softmmu

    addr = m_builder.CreateZExt(addr, wordType());
    addr = m_builder.CreateAdd(addr,
        ConstantInt::get(wordType(), GUEST_BASE));
    addr = m_builder.CreateIntToPtr(addr, intPtrType(bits));
    if(ld) {
        return m_builder.CreateLoad(addr);
    } else {
        m_builder.CreateStore(value, addr);
        return NULL;
    }
#endif // CONFIG_SOFTMMU
}

/*
 * Reference from qeum-2.3: tcg/xxx/tcg-target.c
 static void * const qemu_ld_helpers[16] = {
    [MO_UB]   = helper_ret_ldub_mmu,
    [MO_SB]   = helper_ret_ldsb_mmu,

    [MO_LEUW] = helper_le_lduw_mmu,
    [MO_LEUL] = helper_le_ldul_mmu,
    [MO_LEQ]  = helper_le_ldq_mmu,
    [MO_LESW] = helper_le_ldsw_mmu,
    [MO_LESL] = helper_le_ldul_mmu,

    [MO_BEUW] = helper_be_lduw_mmu,
    [MO_BEUL] = helper_be_ldul_mmu,
    [MO_BEQ]  = helper_be_ldq_mmu,
    [MO_BESW] = helper_be_ldsw_mmu,
    [MO_BESL] = helper_be_ldul_mmu,
};

static void * const qemu_st_helpers[16] = {
    [MO_UB]   = helper_ret_stb_mmu,
    [MO_LEUW] = helper_le_stw_mmu,
    [MO_LEUL] = helper_le_stl_mmu,
    [MO_LEQ]  = helper_le_stq_mmu,
    [MO_BEUW] = helper_be_stw_mmu,
    [MO_BEUL] = helper_be_stl_mmu,
    [MO_BEQ]  = helper_be_stq_mmu,
};
 */

static std::string get_qemu_memo_helper_name(bool ld, TCGArg memop, int bits)
{
    std::string callee_name;
    assert(bits == 32 || bits == 64);

    if(ld) {
        switch(memop){
        case MO_UB:
            callee_name = "helper_ret_ldub_mmu";
            break;
        case MO_SB:
            callee_name = "helper_ret_ldsb_mmu";
            break;
        case MO_LEUW:
            callee_name = "helper_le_lduw_mmu";
            break;
        case MO_LESW:
            callee_name = "helper_le_ldsw_mmu";
            break;
        case MO_LEUL:
            callee_name = "helper_le_ldul_mmu";
            break;
        case MO_LESL:
            callee_name = "helper_le_ldsl_mmu";
            break;
        case MO_LEQ:
            callee_name = "helper_le_ldq_mmu";
            break;
        case MO_BEUW:
            callee_name = "helper_be_lduw_mmu";
            break;
        case MO_BESW:
            callee_name = "helper_be_ldsw_mmu";
            break;
        case MO_BEUL:
            callee_name = "helper_be_ldul_mmu";
            break;
        case MO_BESL:
            callee_name = "helper_be_ldsl_mmu";
            break;
        case MO_BEQ:
            callee_name = "helper_be_ldq_mmu";
            break;
        default:
            assert(false);
        }
    } else {
        switch (memop) {
        case MO_UB:
            callee_name = "helper_ret_stb_mmu";
            break;
        case MO_LEUW:
            callee_name = "helper_le_stw_mmu";
            break;
        case MO_LEUL:
            callee_name = "helper_le_stl_mmu";
            break;
        case MO_LEQ:
            callee_name = "helper_le_stq_mmu";
            break;
        case MO_BEUW:
            callee_name = "helper_be_stw_mmu";
            break;
        case MO_BEUL:
            callee_name = "helper_be_stl_mmu";
            break;
        case MO_BEQ:
            callee_name = "helper_be_stq_mmu";
            break;
        default:
            assert(false);
        }
    }

    return callee_name;
}

inline Value* TCGLLVMContextPrivate::getLdMOValue(Value *value, TCGArg memop, int bits)
{
    // std::cerr << "getLdMOValue() is invokded\n";
    assert(bits <= 64);

    bool is_signed = (memop && MO_SIGN);
    Value* ret = NULL;

    if(is_signed) {
        ret = m_builder.CreateSExt(value, intType(std::max(TARGET_LONG_BITS, 64)));
    } else {
        ret = m_builder.CreateZExt(value, intType(std::max(TARGET_LONG_BITS, 64)));

//        ret = m_builder.CreateZExtOrTrunc(value, intType(std::max(TARGET_LONG_BITS, bits)));
    }

    if(bits < 64) {
        ret = m_builder.CreateTrunc(ret, intType(bits));
        std::cerr << "CRETE WARNING: CreateTrunc is invokded!\n";
    }
    assert(ret != NULL);

    return ret;
}

inline Value* TCGLLVMContextPrivate::new_generateQemuMemOp(bool ld,
        Value *value, Value *addr, TCGArg memop, int mem_index, int bits)
{
    std::cerr << "new_generateQemuMemOp() is invokded\n";

    assert(addr->getType() == intType(TARGET_LONG_BITS));
    assert(ld || value->getType() == intType(bits));
    assert(TCG_TARGET_REG_BITS == 64); //XXX

    std::string callee_name = get_qemu_memo_helper_name(ld, memop, bits);

    std::cerr << "memo_function_name = " << callee_name << std::endl;

    Function* callee = m_module->getFunction(callee_name);
    assert(callee != NULL);

    // Prepare input arguments for qemu memory opeartion helper functions:
    //    helper_ld_*(env, taddr, mmuidx, retaddr)
    //    helper_st_*(env, taddr, value, mmuidx, retaddr)
    std::vector<Value*> i_args;
    Value *Func = callee;
    FunctionType *FTy =
      cast<FunctionType>(cast<PointerType>(Func->getType())->getElementType());

    // "env", a dummy value NULL is assigned here, as env will not be used
    // in helper_ld/st_* functions in offline replay
    i_args.push_back(ConstantPointerNull::get(cast<PointerType>(FTy->getParamType(0))));
    // "taddr"
    i_args.push_back(addr);
    // "value", is required only for helper_st_*
    if(!ld) {
        assert(value != NULL);
        i_args.push_back(value);
    }
    //mem_index
    i_args.push_back(ConstantInt::get(intType(8*sizeof(int)), mem_index));
    // "retaddr", a dummy value 0 is assigned here
    i_args.push_back(ConstantInt::get(intType(8*sizeof(int)), 0));

    // Check type and do cast if necessary:
    //     1. value: sometimes, the input value of helper_st_* requires a cast
    //     2. retaddr
    assert(i_args.size() == FTy->getNumParams());
    for (unsigned i = 0; i != i_args.size(); ++i) {
        if (FTy->getParamType(i) != i_args[i]->getType()){
            i_args[i] = m_builder.CreateIntCast(i_args[i],
                    FTy->getParamType(i), false);

            fprintf(stderr, "A cast is done in qemu memory operation function:%s, i_args[%d].\n",
                    callee_name.c_str(), i);
        }
    }

    Value* result = m_builder.CreateCall(callee,
            ArrayRef<Value*>(i_args));

    if(ld){
        return result;
    } else{
        return NULL;
    }
}

void TCGLLVMContextPrivate::generateTraceCall(uintptr_t pc)
{
}

int TCGLLVMContextPrivate::generateOperation(TCGOp* op, int opc, const TCGArg *args)
{
    Value *v;
    TCGOpDef &def = tcg_op_defs[opc];
    int nb_args = def.nb_args;
//    TCGArg *args = &gen_opparam_buf[op->args];

    std::cerr << "opc: " << def.name << std::endl;
    switch(opc) {
    case INDEX_op_debug_insn_start:
        break;

    /* predefined ops */
    case INDEX_op_nop:
    case INDEX_op_nop1:
    case INDEX_op_nop2:
    case INDEX_op_nop3:
        break;

    case INDEX_op_nopn:
        nb_args = args[0];
        break;

    case INDEX_op_discard:
        delValue(args[0]);
        break;

    case INDEX_op_call:
        {
            // The order in arg[] is :
            // out args, input args, function addr, flag

//            std::cerr << "INDEX_op_call\n" << std::endl;

            const TCGOpDef *def;
            def = &tcg_op_defs[INDEX_op_call];

            int nb_oargs = op->callo;
            int nb_iargs = op->calli;
            int nb_cargs = def->nb_cargs;
            int flags = args[nb_oargs + nb_iargs + 1];
                        assert((flags & TCG_CALL_TYPE_MASK) == TCG_CALL_TYPE_STD);

            nb_args = nb_oargs + nb_iargs + 2;

//            fprintf(stderr, "nb_oargs = %d, nb_iargs = %d\n",
//                    nb_oargs, nb_iargs);

            std::vector<Value*> argValues;
            std::vector<llvm::Type*> argTypes;
            argValues.reserve(nb_iargs);
            argTypes.reserve(nb_iargs);
            for(int i=0; i < nb_iargs; ++i) {
                TCGArg arg = args[nb_oargs + i];
                if(arg != TCG_CALL_DUMMY_ARG) {
                    Value *v = getValue(arg);
                    argValues.push_back(v);
                    argTypes.push_back(v->getType());
                }
            }

            assert(nb_oargs == 0 || nb_oargs == 1);
            llvm::Type* retType = nb_oargs == 0 ?
                llvm::Type::getVoidTy(m_context) : wordType(getValueBits(args[1]));

            tcg_target_ulong helperAddrC = (tcg_target_ulong) args[nb_oargs + nb_iargs];
            const string helperName = get_crete_helper_name((uint64_t)helperAddrC);

            std::string funcName = std::string("helper_") + helperName;
            Function* helperFunc = m_module->getFunction(funcName);
            if(!helperFunc) {
                helperFunc = Function::Create(
                        FunctionType::get(retType, argTypes, false),
                        Function::ExternalLinkage, funcName, m_module);
                m_executionEngine->addGlobalMapping(helperFunc,
                                                    (void*) helperAddrC);
                /* XXX: Why do we need this ? */
                sys::DynamicLibrary::AddSymbol(funcName, (void*) helperAddrC);
            }

            Value *Func = helperFunc;
            FunctionType *FTy =
              cast<FunctionType>(cast<PointerType>(Func->getType())->getElementType());
            assert(argValues.size() == FTy->getNumParams());
            for (unsigned i = 0; i != argValues.size(); ++i) {
                if (FTy->getParamType(i) != argValues[i]->getType()){
                    argValues[i] = m_builder.CreateIntToPtr(argValues[i],
                            FTy->getParamType(i));

                    fprintf(stderr, "A cast is done for calling function %s.\n",
                            funcName.c_str());
                }
            }

            Value* result = m_builder.CreateCall(helperFunc,
                    ArrayRef<Value*>(argValues));


            /* Invalidate in-memory values because
             * function might have changed them */
            for(int i=0; i<m_tcgContext->nb_globals; ++i)
                delValue(i);

            for(int i=m_tcgContext->nb_globals; i<TCG_MAX_TEMPS; ++i)
                if(m_tcgContext->temps[i].temp_local)
                    delValue(i);

            /* Invalidate all pointers to globals */
            for(int i=0; i<m_tcgContext->nb_globals; ++i)
                delPtrForValue(i);

            if(nb_oargs == 1)
                setValue(args[0], result);
        }
        break;

    case INDEX_op_br:
        m_builder.CreateBr(getLabel(args[0]));
        startNewBasicBlock();
        break;

#define __OP_BRCOND_C(tcg_cond, cond)                               \
            case tcg_cond:                                          \
                v = m_builder.CreateICmp ## cond(                   \
                        getValue(args[0]), getValue(args[1]));      \
            break;

#define __OP_BRCOND(opc_name, bits)                                 \
    case opc_name: {                                                \
        assert(getValue(args[0])->getType() == intType(bits));      \
        assert(getValue(args[1])->getType() == intType(bits));      \
        switch(args[2]) {                                           \
            __OP_BRCOND_C(TCG_COND_EQ,   EQ)                        \
            __OP_BRCOND_C(TCG_COND_NE,   NE)                        \
            __OP_BRCOND_C(TCG_COND_LT,  SLT)                        \
            __OP_BRCOND_C(TCG_COND_GE,  SGE)                        \
            __OP_BRCOND_C(TCG_COND_LE,  SLE)                        \
            __OP_BRCOND_C(TCG_COND_GT,  SGT)                        \
            __OP_BRCOND_C(TCG_COND_LTU, ULT)                        \
            __OP_BRCOND_C(TCG_COND_GEU, UGE)                        \
            __OP_BRCOND_C(TCG_COND_LEU, ULE)                        \
            __OP_BRCOND_C(TCG_COND_GTU, UGT)                        \
            default:                                                \
                tcg_abort();                                        \
        }                                                           \
        BasicBlock* bb = BasicBlock::Create(m_context);             \
        m_builder.CreateCondBr(v, getLabel(args[3]), bb);           \
        startNewBasicBlock(bb);                                     \
    } break;

    __OP_BRCOND(INDEX_op_brcond_i32, 32)

#if TCG_TARGET_REG_BITS == 64
    __OP_BRCOND(INDEX_op_brcond_i64, 64)
#endif

#undef __OP_BRCOND_C
#undef __OP_BRCOND

#define __OP_SETCOND_C(tcg_cond, cond)                               \
            case tcg_cond:                                          \
                v = m_builder.CreateICmp ## cond(                   \
                        getValue(args[1]), getValue(args[2]));      \
            break;

#define __OP_SETCOND(opc_name, bits)                                 \
    case opc_name: {                                                \
        assert(getValue(args[1])->getType() == intType(bits));      \
        assert(getValue(args[2])->getType() == intType(bits));      \
        switch(args[3]) {                                           \
            __OP_SETCOND_C(TCG_COND_EQ,   EQ)                        \
            __OP_SETCOND_C(TCG_COND_NE,   NE)                        \
            __OP_SETCOND_C(TCG_COND_LT,  SLT)                        \
            __OP_SETCOND_C(TCG_COND_GE,  SGE)                        \
            __OP_SETCOND_C(TCG_COND_LE,  SLE)                        \
            __OP_SETCOND_C(TCG_COND_GT,  SGT)                        \
            __OP_SETCOND_C(TCG_COND_LTU, ULT)                        \
            __OP_SETCOND_C(TCG_COND_GEU, UGE)                        \
            __OP_SETCOND_C(TCG_COND_LEU, ULE)                        \
            __OP_SETCOND_C(TCG_COND_GTU, UGT)                        \
            default:                                                \
                tcg_abort();                                        \
        }                                                           \
        setValue(args[0], m_builder.CreateZExt(v, intType(bits)));  \
    } break;

    __OP_SETCOND(INDEX_op_setcond_i32, 32)

#if TCG_TARGET_REG_BITS == 64
    __OP_SETCOND(INDEX_op_setcond_i64, 64)
#endif

#undef __OP_SETCOND_C
#undef __OP_SETCOND

    case INDEX_op_set_label:
        assert(getLabel(args[0])->getParent() == 0);
        startNewBasicBlock(getLabel(args[0]));
        break;

    case INDEX_op_movi_i32:
        setValue(args[0], ConstantInt::get(intType(32), args[1]));
        break;

    case INDEX_op_mov_i32:
        // Move operation may perform truncation of the value
        assert(getValue(args[1])->getType() == intType(32) ||
                getValue(args[1])->getType() == intType(64));
        setValue(args[0],
                m_builder.CreateTrunc(getValue(args[1]), intType(32)));
        break;

#if TCG_TARGET_REG_BITS == 64
    case INDEX_op_movi_i64:
        setValue(args[0], ConstantInt::get(intType(64), args[1]));
        break;

    case INDEX_op_mov_i64:
        assert(getValue(args[1])->getType() == intType(64));
        setValue(args[0], getValue(args[1]));
        break;
#endif

    /* size extensions */
#define __EXT_OP(opc_name, truncBits, opBits, signE )               \
    case opc_name:                                                  \
        /*                                                          \
        assert(getValue(args[1])->getType() == intType(opBits) ||   \
               getValue(args[1])->getType() == intType(truncBits)); \
        */                                                          \
        setValue(args[0], m_builder.Create ## signE ## Ext(         \
                m_builder.CreateTrunc(                              \
                    getValue(args[1]), intType(truncBits)),         \
                intType(opBits)));                                  \
        break;

    __EXT_OP(INDEX_op_ext8s_i32,   8, 32, S)
    __EXT_OP(INDEX_op_ext8u_i32,   8, 32, Z)
    __EXT_OP(INDEX_op_ext16s_i32, 16, 32, S)
    __EXT_OP(INDEX_op_ext16u_i32, 16, 32, Z)

#if TCG_TARGET_REG_BITS == 64
    __EXT_OP(INDEX_op_ext8s_i64,   8, 64, S)
    __EXT_OP(INDEX_op_ext8u_i64,   8, 64, Z)
    __EXT_OP(INDEX_op_ext16s_i64, 16, 64, S)
    __EXT_OP(INDEX_op_ext16u_i64, 16, 64, Z)
    __EXT_OP(INDEX_op_ext32s_i64, 32, 64, S)
    __EXT_OP(INDEX_op_ext32u_i64, 32, 64, Z)
#endif

#undef __EXT_OP

    /* load/store */
#define __LD_OP(opc_name, memBits, regBits, signE)                  \
    case opc_name:                                                  \
        assert(getValue(args[1])->getType() == wordType());         \
        v = m_builder.CreateAdd(getValue(args[1]),                  \
                    ConstantInt::get(wordType(), args[2]));         \
        v = m_builder.CreateIntToPtr(v, intPtrType(memBits));       \
        v = m_builder.CreateLoad(v);                                \
        setValue(args[0], m_builder.Create ## signE ## Ext(         \
                    v, intType(regBits)));                          \
        break;

#ifdef TARGET_ARM

#define __ST_OP(opc_name, memBits, regBits)                         \
    case opc_name:  {                                                 \
        assert(getValue(args[0])->getType() == intType(regBits));   \
        assert(getValue(args[1])->getType() == wordType());         \
        Value* valueToStore = getValue(args[0]);                    \
                                                                    \
        if (TARGET_LONG_BITS == memBits && !execute_llvm            \
            && args[1] == 0                                         \
            && args[2] == offsetof(CPUARMState, regs[15])) {        \
            valueToStore = handleSymbolicPcAssignment(valueToStore);\
        }                                                           \
                                                                    \
        v = m_builder.CreateAdd(getValue(args[1]),                  \
                    ConstantInt::get(wordType(), args[2]));         \
        v = m_builder.CreateIntToPtr(v, intPtrType(memBits));       \
        m_builder.CreateStore(m_builder.CreateTrunc(                \
                valueToStore, intType(memBits)), v);           \
    } break;

#elif defined(TARGET_I386)

#define __ST_OP(opc_name, memBits, regBits)                         \
    case opc_name:  {                                                 \
        assert(getValue(args[0])->getType() == intType(regBits));   \
        assert(getValue(args[1])->getType() == wordType());         \
        Value* valueToStore = getValue(args[0]);                    \
                                                                    \
        if (TARGET_LONG_BITS == memBits && !execute_llvm            \
            && args[1] == 0                                         \
            && args[2] == offsetof(CPUX86State, eip)) { \
            valueToStore = handleSymbolicPcAssignment(valueToStore);\
        }                                                           \
                                                                    \
        v = m_builder.CreateAdd(getValue(args[1]),                  \
                    ConstantInt::get(wordType(), args[2]));         \
        v = m_builder.CreateIntToPtr(v, intPtrType(memBits));       \
        m_builder.CreateStore(m_builder.CreateTrunc(                \
                valueToStore, intType(memBits)), v);           \
    } break;

#endif


    __LD_OP(INDEX_op_ld8u_i32,   8, 32, Z)
    __LD_OP(INDEX_op_ld8s_i32,   8, 32, S)
    __LD_OP(INDEX_op_ld16u_i32, 16, 32, Z)
    __LD_OP(INDEX_op_ld16s_i32, 16, 32, S)
    __LD_OP(INDEX_op_ld_i32,    32, 32, Z)

    __ST_OP(INDEX_op_st8_i32,   8, 32)
    __ST_OP(INDEX_op_st16_i32, 16, 32)
    __ST_OP(INDEX_op_st_i32,   32, 32)

#if TCG_TARGET_REG_BITS == 64
    __LD_OP(INDEX_op_ld8u_i64,   8, 64, Z)
    __LD_OP(INDEX_op_ld8s_i64,   8, 64, S)
    __LD_OP(INDEX_op_ld16u_i64, 16, 64, Z)
    __LD_OP(INDEX_op_ld16s_i64, 16, 64, S)
    __LD_OP(INDEX_op_ld32u_i64, 32, 64, Z)
    __LD_OP(INDEX_op_ld32s_i64, 32, 64, S)
    __LD_OP(INDEX_op_ld_i64,    64, 64, Z)

    __ST_OP(INDEX_op_st8_i64,   8, 64)
    __ST_OP(INDEX_op_st16_i64, 16, 64)
    __ST_OP(INDEX_op_st32_i64, 32, 64)
    __ST_OP(INDEX_op_st_i64,   64, 64)
#endif

#undef __LD_OP
#undef __ST_OP

    /* arith */
#define __ARITH_OP(opc_name, op, bits)                              \
    case opc_name: {                                                \
        Value *v1 = getValue(args[1]);                              \
        Value *v2 = getValue(args[2]);                              \
        adjustTypeSize(bits, &v1, &v2);                             \
        assert(v1->getType() == intType(bits));                     \
        assert(v2->getType() == intType(bits));                     \
        setValue(args[0], m_builder.Create ## op(v1, v2));          \
    } break;

#define __ARITH_OP_DIV2(opc_name, signE, bits)                      \
    case opc_name:                                                  \
        assert(getValue(args[2])->getType() == intType(bits));      \
        assert(getValue(args[3])->getType() == intType(bits));      \
        assert(getValue(args[4])->getType() == intType(bits));      \
        v = m_builder.CreateShl(                                    \
                m_builder.CreateZExt(                               \
                    getValue(args[3]), intType(bits*2)),            \
                m_builder.CreateZExt(                               \
                    ConstantInt::get(intType(bits), bits),          \
                    intType(bits*2)));                              \
        v = m_builder.CreateOr(v,                                   \
                m_builder.CreateZExt(                               \
                    getValue(args[2]), intType(bits*2)));           \
        setValue(args[0], m_builder.Create ## signE ## Div(         \
                v, getValue(args[4])));                             \
        setValue(args[1], m_builder.Create ## signE ## Rem(         \
                v, getValue(args[4])));                             \
        break;

#define __ARITH_OP_ROT(opc_name, op1, op2, bits)                    \
    case opc_name:                                                  \
        assert(getValue(args[1])->getType() == intType(bits));      \
        assert(getValue(args[2])->getType() == intType(bits));      \
        v = m_builder.CreateSub(                                    \
                ConstantInt::get(intType(bits), bits),              \
                getValue(args[2]));                                 \
        setValue(args[0], m_builder.CreateOr(                       \
                m_builder.Create ## op1 (                           \
                    getValue(args[1]), getValue(args[2])),          \
                m_builder.Create ## op2 (                           \
                    getValue(args[1]), v)));                        \
        break;

#define __ARITH_OP_I(opc_name, op, i, bits)                         \
    case opc_name:                                                  \
        assert(getValue(args[1])->getType() == intType(bits));      \
        setValue(args[0], m_builder.Create ## op(                   \
                    ConstantInt::get(intType(bits), i),             \
                    getValue(args[1])));                            \
        break;

#define __ARITH_OP_BSWAP(opc_name, sBits, bits)                     \
    case opc_name: {                                                \
        assert(getValue(args[1])->getType() == intType(bits));      \
        llvm::Type* Tys[] = { intType(sBits) };                     \
        Function *bswap = Intrinsic::getDeclaration(m_module,       \
                Intrinsic::bswap, ArrayRef<llvm::Type*>(Tys,1));                          \
        v = m_builder.CreateTrunc(getValue(args[1]),intType(sBits));\
        setValue(args[0], m_builder.CreateZExt(                     \
                m_builder.CreateCall(bswap, v), intType(bits)));    \
        } break;


    __ARITH_OP(INDEX_op_add_i32, Add, 32)
    __ARITH_OP(INDEX_op_sub_i32, Sub, 32)
    __ARITH_OP(INDEX_op_mul_i32, Mul, 32)

#ifdef TCG_TARGET_HAS_div_i32
    __ARITH_OP(INDEX_op_div_i32,  SDiv, 32)
    __ARITH_OP(INDEX_op_divu_i32, UDiv, 32)
    __ARITH_OP(INDEX_op_rem_i32,  SRem, 32)
    __ARITH_OP(INDEX_op_remu_i32, URem, 32)
#else
    __ARITH_OP_DIV2(INDEX_op_div2_i32,  S, 32)
    __ARITH_OP_DIV2(INDEX_op_divu2_i32, U, 32)
#endif

    __ARITH_OP(INDEX_op_and_i32, And, 32)
    __ARITH_OP(INDEX_op_or_i32,   Or, 32)
    __ARITH_OP(INDEX_op_xor_i32, Xor, 32)

    __ARITH_OP(INDEX_op_shl_i32,  Shl, 32)
    __ARITH_OP(INDEX_op_shr_i32, LShr, 32)
    __ARITH_OP(INDEX_op_sar_i32, AShr, 32)

    __ARITH_OP_ROT(INDEX_op_rotl_i32, Shl, LShr, 32)
    __ARITH_OP_ROT(INDEX_op_rotr_i32, LShr, Shl, 32)

    __ARITH_OP_I(INDEX_op_not_i32, Xor, (uint64_t) -1, 32)
    __ARITH_OP_I(INDEX_op_neg_i32, Sub, 0, 32)

    __ARITH_OP_BSWAP(INDEX_op_bswap16_i32, 16, 32)
    __ARITH_OP_BSWAP(INDEX_op_bswap32_i32, 32, 32)

#if TCG_TARGET_REG_BITS == 64
    __ARITH_OP(INDEX_op_add_i64, Add, 64)
    __ARITH_OP(INDEX_op_sub_i64, Sub, 64)
    __ARITH_OP(INDEX_op_mul_i64, Mul, 64)

#ifdef TCG_TARGET_HAS_div_i64
    __ARITH_OP(INDEX_op_div_i64,  SDiv, 64)
    __ARITH_OP(INDEX_op_divu_i64, UDiv, 64)
    __ARITH_OP(INDEX_op_rem_i64,  SRem, 64)
    __ARITH_OP(INDEX_op_remu_i64, URem, 64)
#else
    __ARITH_OP_DIV2(INDEX_op_div2_i64,  S, 64)
    __ARITH_OP_DIV2(INDEX_op_divu2_i64, U, 64)
#endif

    __ARITH_OP(INDEX_op_and_i64, And, 64)
    __ARITH_OP(INDEX_op_or_i64,   Or, 64)
    __ARITH_OP(INDEX_op_xor_i64, Xor, 64)

    __ARITH_OP(INDEX_op_shl_i64,  Shl, 64)
    __ARITH_OP(INDEX_op_shr_i64, LShr, 64)
    __ARITH_OP(INDEX_op_sar_i64, AShr, 64)

    __ARITH_OP_ROT(INDEX_op_rotl_i64, Shl, LShr, 64)
    __ARITH_OP_ROT(INDEX_op_rotr_i64, LShr, Shl, 64)

    __ARITH_OP_I(INDEX_op_not_i64, Xor, (uint64_t) -1, 64)
    __ARITH_OP_I(INDEX_op_neg_i64, Sub, 0, 64)

    __ARITH_OP_BSWAP(INDEX_op_bswap16_i64, 16, 64)
    __ARITH_OP_BSWAP(INDEX_op_bswap32_i64, 32, 64)
    __ARITH_OP_BSWAP(INDEX_op_bswap64_i64, 64, 64)
#endif

#undef __ARITH_OP_BSWAP
#undef __ARITH_OP_I
#undef __ARITH_OP_ROT
#undef __ARITH_OP_DIV2
#undef __ARITH_OP

    /* QEMU specific */
#if TCG_TARGET_REG_BITS == 64
// new set of qemu mem operations
// QEMU 2.3:
//    * qemu_ld_i32/i64 t0, t1, flags, memidx
//    * qemu_st_i32/i64 t0, t1, flags, memidx

#define __NEW_OP_QEMU_LD(opc_name, bits)                         \
    case opc_name:                                               \
        v = new_generateQemuMemOp(true, NULL,                    \
            getValue(args[1]), args[2], args[3], bits);          \
        setValue(args[0], getLdMOValue(v, args[2], bits));       \
        break;

#define __NEW_OP_QEMU_ST(opc_name, bits)                         \
    case opc_name:                                               \
        new_generateQemuMemOp(false,                             \
            m_builder.CreateIntCast(                             \
                getValue(args[0]), intType(bits), false),        \
            getValue(args[1]), args[2], args[3], bits);          \
        break;

    __NEW_OP_QEMU_LD(INDEX_op_qemu_ld_i32, 32)
    __NEW_OP_QEMU_LD(INDEX_op_qemu_ld_i64, 64)
    __NEW_OP_QEMU_ST(INDEX_op_qemu_st_i32, 32)
    __NEW_OP_QEMU_ST(INDEX_op_qemu_st_i64, 64)

#undef __NEW_OP_QEMU_LD
#undef __NEW_OP_QEMU_ST
#endif

    case INDEX_op_exit_tb:
        m_builder.CreateRet(ConstantInt::get(wordType(), args[0]));
        break;

    case INDEX_op_goto_tb:
        /* XXX: tb linking is disabled */
        break;

    case INDEX_op_deposit_i32: {
        //llvm::errs() << *m_tbFunction << "\n";
        Value *arg1 = getValue(args[1]);
        //llvm::errs() << "arg1=" << *arg1 << "\n";
        //arg1 = m_builder.CreateTrunc(arg1, intType(32));


        Value *arg2 = getValue(args[2]);
        //llvm::errs() << "arg2=" << *arg2 << "\n";
        arg2 = m_builder.CreateTrunc(arg2, intType(32));

        uint32_t ofs = args[3];
        uint32_t len = args[4];

        if (ofs == 0 && len == 32) {
            setValue(args[0], arg2);
            break;
        }

        uint32_t mask = (1u << len) - 1;
        Value *t1, *ret;
        if (ofs + len < 32) {
            t1 = m_builder.CreateAnd(arg2, APInt(32, mask));
            t1 = m_builder.CreateShl(t1, APInt(32, ofs));
        } else {
            t1 = m_builder.CreateShl(arg2, APInt(32, ofs));
        }

        ret = m_builder.CreateAnd(arg1, APInt(32, ~(mask << ofs)));
        ret = m_builder.CreateOr(ret, t1);
        setValue(args[0], ret);
    }
    break;
#if TCG_TARGET_REG_BITS == 64
    case INDEX_op_deposit_i64: {
        Value *arg1 = getValue(args[1]);
        Value *arg2 = getValue(args[2]);
        arg2 = m_builder.CreateTrunc(arg2, intType(64));

        uint32_t ofs = args[3];
        uint32_t len = args[4];

        if (0 == ofs && 64 == len) {
            setValue(args[0], arg2);
            break;
        }

        uint64_t mask = (1u << len) - 1;
        Value *t1, *ret;

        if (ofs + len < 64) {
            t1 = m_builder.CreateAnd(arg2, APInt(64, mask));
            t1 = m_builder.CreateShl(t1, APInt(64, ofs));
        } else {
            t1 = m_builder.CreateShl(arg2, APInt(64, ofs));
        }

        ret = m_builder.CreateAnd(arg1, APInt(64, ~(mask << ofs)));
        ret = m_builder.CreateOr(ret, t1);
        setValue(args[0], ret);
    }
    break;
#endif

    default:
        std::cerr << "ERROR: unknown TCG micro operation '"
                  << def.name << "'" << std::endl;
        tcg_abort();
        break;
    }

    return nb_args;
}

void TCGLLVMContextPrivate::generateCode(TCGContext *s, TranslationBlock *tb)
{
    /* Create new function for current translation block */
    /* TODO: compute the checksum of the tb to see if we can reuse some code */
    std::ostringstream fName;
    fName << "tcg-llvm-tb-" << (m_tbCount++) << "-" << std::hex << tb->pc;


#if defined(CRETE_DEBUG)
    std::cerr << " generateCode: " << fName.str()<<'\n';
#endif // defined(CRETE_DEBUG)

    /*
    if(m_tbFunction)
        m_tbFunction->eraseFromParent();
    */

    FunctionType *tbFunctionType = FunctionType::get(
            wordType(),
            std::vector<llvm::Type*>(1, intPtrType(64)), false);

#if defined(TCG_LLVM_OFFLINE)
    // BOBO: change the function's linkage as not "private", so that it could be
    // called by the function of other modules, the harness module
    m_tbFunction = Function::Create(tbFunctionType,
            Function::ExternalLinkage, fName.str(), m_module);

#else //#if defined(TCG_LLVM_OFFLINE)
    m_tbFunction = Function::Create(tbFunctionType,
            Function::PrivateLinkage, fName.str(), m_module);
#endif //#if defined(TCG_LLVM_OFFLINE)

    //Enable the online translation of llvm
    BasicBlock *basicBlock = BasicBlock::Create(m_context,
            "entry", m_tbFunction);
    m_builder.SetInsertPoint(basicBlock);

    m_tcgContext = s;

    /* Prepare globals and temps information */
    initGlobalsAndLocalTemps();

    cerr<< "initGlobalsAndLocalTemps() finished." << endl;
    uint64_t inst_count = 0;

    /* Generate code for each opc */
    const TCGArg *args = gen_opparam_buf;

    TCGOp *op;
    int args_increase_ret = 0;
    int args_increase_valid = 0;
    int args_idx = 0;
    for (int oi = s->gen_first_op_idx, opc_index = 0; oi >= 0; oi = op->next, ++opc_index)
    {
        op = &s->gen_op_buf[oi];

    	// cout << "inst_count: " << dec << inst_count++ << endl;

    	int opc = gen_opc_buf[opc_index];

    	if(args_idx != 0) {
    	    args_increase_valid = op->args - args_idx;
            std::cerr << std::dec << args_increase_ret << ": " << args_increase_valid << std::endl;
            assert(args_increase_ret == args_increase_valid);
    	}

    	args_idx = op->args;
    	args = &gen_opparam_buf[op->args];

        if(opc == INDEX_op_end)
            break;

        if(opc == INDEX_op_debug_insn_start) {
        }

        generateTraceCall(tb->pc);
        args_increase_ret = generateOperation(op, opc, args);
        //llvm::errs() << *m_tbFunction << "\n";
    }

    clearLabels();

    /* Finalize function */
    if(!isa<ReturnInst>(m_tbFunction->back().back()))
        m_builder.CreateRet(ConstantInt::get(wordType(), 0));

    /* Clean up unused m_values */
    for(int i=0; i<TCG_MAX_TEMPS; ++i)
        delValue(i);

    /* Delete pointers after deleting values */
    for(int i=0; i<TCG_MAX_TEMPS; ++i)
        delPtrForValue(i);

    for(int i=0; i<TCG_MAX_LABELS; ++i)
        delLabel(i);

#ifndef NDEBUG
    verifyFunction(*m_tbFunction);
#endif

    //KLEE will optimize the function later
    //m_functionPassManager->run(*m_tbFunction);

    tb->llvm_function = m_tbFunction;

    if(execute_llvm || qemu_loglevel_mask(CPU_LOG_LLVM_ASM)) {
        tb->llvm_tc_ptr = (uint8_t*)
                m_executionEngine->getPointerToFunction(m_tbFunction);
        tb->llvm_tc_end = tb->llvm_tc_ptr +
                m_jitMemoryManager->getLastFunctionSize();
    } else {
        tb->llvm_tc_ptr = 0;
        tb->llvm_tc_end = 0;
    }

#ifdef DEBUG_DISAS
    if (unlikely(qemu_loglevel_mask(CPU_LOG_TB_OP))) {
        qemu_log("OP:\n");
        tcg_dump_ops(s, logfile);
        qemu_log("\n");
    }
#endif

    if(qemu_loglevel_mask(CPU_LOG_LLVM_IR)) {
        std::string fcnString;
        llvm::raw_string_ostream s(fcnString);
        s << *m_tbFunction;
        qemu_log("OUT (LLVM IR):\n");
        qemu_log("%s", s.str().c_str());
        qemu_log("\n");
        qemu_log_flush();
    }
}

void TCGLLVMContextPrivate::crete_init_helper_names(const map<uint64_t, string>& helper_names)
{
    m_crete_helper_names = helper_names;
}

const string TCGLLVMContextPrivate::get_crete_helper_name(const uint64_t func_addr) const
{
    map<uint64_t, string>::const_iterator it = m_crete_helper_names.find(func_addr);
    assert(it != m_crete_helper_names.end() &&
            "[CRETE ERROR] Can not find the helper function name based the given func_addr.\n");

    return it->second;
}

void TCGLLVMContextPrivate::crete_set_cpuState_size(uint64_t cpuState_size)
{
    m_cpuState_size = cpuState_size;
}

void TCGLLVMContextPrivate::crete_add_tbExecSequ(vector<pair<uint64_t, uint64_t> > seq)
{
    m_tbExecSequ.insert(m_tbExecSequ.end(), seq.begin(), seq.end());
}

//#define CRETE_CROSS_CHECK

void TCGLLVMContextPrivate::generate_crete_main()
{
    FunctionType *FT = FunctionType::get(Type::getVoidTy(m_context),
            std::vector<llvm::Type*>(0), false);

    Function *crete_main_func = Function::Create(FT,
            Function::PrivateLinkage, "main", m_module);

    BasicBlock *basicBlock = BasicBlock::Create(m_context,
            "entry", crete_main_func);
    m_builder.SetInsertPoint(basicBlock);

    ArrayType* tcg_env_type = ArrayType::get(intType(8), m_cpuState_size);

#if defined(CRETE_CROSS_CHECK)
    // 0. call void @crete_verify_all_cpuState_offset()
    Function *crete_verify_all_cpuState_offset = m_module->getFunction("crete_verify_all_cpuState_offset");
    assert(crete_verify_all_cpuState_offset);

    m_builder.CreateCall(crete_verify_all_cpuState_offset, std::vector<Value*>());
#endif

    // 1. %cpu_state = alloca [34320 x i8]
    Value *cpu_state= m_builder.CreateAlloca(tcg_env_type, 0, "cpu_state");

    // 2. call void @init_cpu_state([34320 x i8]* %cpu_state)
    {
        std::vector<Value*> argValues;
        std::vector<llvm::Type*> argTypes;

        argValues.push_back(cpu_state);
        argTypes.push_back(cpu_state->getType());

        Function *init_cpu_state = Function::Create(
                FunctionType::get(Type::getVoidTy(m_context), argTypes, false),
                Function::ExternalLinkage, "crete_init_cpu_state", m_module);

        m_builder.CreateCall(init_cpu_state,
                            ArrayRef<Value*>(argValues));
    }

    // 3. %cpu_state_addr = alloca i64
    Value *cpu_state_addr = m_builder.CreateAlloca(intType(64), 0, "cpu_state_addr");

    // 4.   %0 = ptrtoint [34320 x i8]* %cpu_state to i64
    //      store i64 %0, i64* %cpu_state_addr
    m_builder.CreateStore(m_builder.CreatePtrToInt(cpu_state, intType(64)),
            cpu_state_addr, false);

    std::vector<llvm::Type*> tb_prologue_argTypes;
    tb_prologue_argTypes.push_back(intType(64));
    tb_prologue_argTypes.push_back(intType(64));

    Function *crete_qemu_tb_prologue = Function::Create(
            FunctionType::get(Type::getVoidTy(m_context), tb_prologue_argTypes, false),
                    Function::ExternalLinkage, "crete_qemu_tb_prologue", m_module);

    uint64_t tb_count = 0;
    for(vector<pair<uint64_t, uint64_t> >::const_iterator it = m_tbExecSequ.begin();
            it != m_tbExecSequ.end(); ++it) {
        // 5.   call void @crete_qemu_tb_prologue(i64 tb_count, i64 tb_pc)
        std::vector<Value*> tb_prologue_argValues;
        tb_prologue_argValues.push_back(ConstantInt::get(intType(64), tb_count++));
        tb_prologue_argValues.push_back(ConstantInt::get(intType(64), it->first));
        m_builder.CreateCall(crete_qemu_tb_prologue,tb_prologue_argValues);

        // 6.   %1 = call i64 @tcg-llvm-tb-0-b7db3f45(i64* %cpu_state_addr)
        std::ostringstream fName;
        fName << "tcg-llvm-tb-" << std::dec << it->second << "-" << std::hex << it->first;
        Function *tcg_llvm_tb = m_module->getFunction(fName.str());

        assert(tcg_llvm_tb);

        m_builder.CreateCall(tcg_llvm_tb,
                std::vector<llvm::Value*>(1, cpu_state_addr));
    }

    Function *crete_finish_replay = Function::Create(
            FunctionType::get(Type::getVoidTy(m_context),
                    std::vector<llvm::Type*>(1, intType(64)), false),
                    Function::ExternalLinkage, "crete_finish_replay", m_module);
    m_builder.CreateCall(crete_finish_replay,
            std::vector<llvm::Value*>(1, ConstantInt::get(intType(64), tb_count)));

    m_builder.CreateRet(0);
}

/***********************************/
/* External interface for C++ code */

TCGLLVMContext::TCGLLVMContext()
        : m_private(new TCGLLVMContextPrivate)
{
}

TCGLLVMContext::~TCGLLVMContext()
{
    delete m_private;
}

llvm::FunctionPassManager* TCGLLVMContext::getFunctionPassManager() const
{
    return m_private->getFunctionPassManager();
}

void TCGLLVMContext::deleteExecutionEngine()
{
    m_private->deleteExecutionEngine();
}

LLVMContext& TCGLLVMContext::getLLVMContext()
{
    return m_private->m_context;
}

Module* TCGLLVMContext::getModule()
{
    return m_private->m_module;
}

ExecutionEngine* TCGLLVMContext::getExecutionEngine()
{
    return m_private->m_executionEngine;
}


void TCGLLVMContext::initializeHelpers()
{
    return m_private->initializeHelpers();
}


void TCGLLVMContext::generateCode(TCGContext *s, TranslationBlock *tb)
{
    assert(tb->tcg_llvm_context == NULL);
    assert(tb->llvm_function == NULL);

    tb->tcg_llvm_context = this;
    m_private->generateCode(s, tb);
}

#if defined(TCG_LLVM_OFFLINE)
int TCGLLVMContext::getTbCount(){
	return m_private->m_tbCount;
}

void TCGLLVMContext::writeBitCodeToFile(const std::string &fileName) {
	assert(fileName.c_str());
	m_private->writeBitCodeToFile(fileName);
}

void TCGLLVMContext::linkWithLibrary(const std::string& libraryName)
{
	llvm::Linker linker("tcg_llvm_ctx", getModule(), false);
	llvm::sys::Path libraryPath(libraryName);
	bool native = false;

	  if (linker.LinkInFile(libraryPath, native)) {
	    assert(0 && "linking in library failed!");
	  }

	 linker.releaseModule();
}

void TCGLLVMContext::crete_init_helper_names(const map<uint64_t, string>& helper_names)
{
    m_private->crete_init_helper_names(helper_names);
}

const string TCGLLVMContext::get_crete_helper_name(const uint64_t func_addr) const
{
    return m_private->get_crete_helper_name(func_addr);
}

void TCGLLVMContext::crete_set_cpuState_size(uint64_t cpuState_size)
{
    m_private->crete_set_cpuState_size(cpuState_size);
}

void TCGLLVMContext::crete_add_tbExecSequ(vector<pair<uint64_t, uint64_t> > seq)
{
    m_private->crete_add_tbExecSequ(seq);
}

void TCGLLVMContext::generate_crete_main()
{
    m_private->generate_crete_main();
}
#endif
/*****************************/
/* Functions for QEMU c code */

TCGLLVMContext* tcg_llvm_initialize()
{
    if (!llvm_start_multithreaded()) {
        fprintf(stderr, "Could not initialize LLVM threading\n");
        exit(-1);
    }
    return new TCGLLVMContext;
}

void tcg_llvm_close(TCGLLVMContext *l)
{
    delete l;
}

void tcg_llvm_gen_code(TCGLLVMContext *l, TCGContext *s, TranslationBlock *tb)
{
    l->generateCode(s, tb);
}

void tcg_llvm_tb_alloc(TranslationBlock *tb)
{
    tb->tcg_llvm_context = NULL;
    tb->llvm_function = NULL;
}

void tcg_llvm_tb_free(TranslationBlock *tb)
{
    if(tb->llvm_function) {
        tb->llvm_function->eraseFromParent();
    }
}

const char* tcg_llvm_get_func_name(TranslationBlock *tb)
{
    static char buf[64];
    if(tb->llvm_function) {
        strncpy(buf, tb->llvm_function->getName().str().c_str(), sizeof(buf));
    } else {
        buf[0] = 0;
    }
    return buf;
}

void tcg_llvm_initHelper(struct TCGLLVMContext *l)
{
	l->initializeHelpers();
}

#ifdef TCG_LLVM_OFFLINE
int get_llvm_tbCount(struct TCGLLVMContext *l)
{
	return l->getTbCount();
}

void tcg_linkWithLibrary(struct TCGLLVMContext *l, const char *libraryName)
{
	if(libraryName)
		l->linkWithLibrary(libraryName);
}
#else
#error
#endif
