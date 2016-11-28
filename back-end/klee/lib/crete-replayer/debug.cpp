#include "crete-replayer/debug.h"
#include "crete-replayer/qemu_rt_info.h"

#include "klee/util/ExprPPrinter.h"
#include "klee/ExecutionState.h"
#include "klee/Constraints.h"

#include "../../lib/Core/AddressSpace.h"
#include "../../lib/Core/MemoryManager.h"
#include "../../lib/Core/Memory.h"

#include "llvm/Instruction.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <assert.h>

namespace fs = llvm::sys::fs;

namespace crete
{
namespace debug
{

void throw_exception_file(ExceptionFlag flag,
                          const std::string& msg,
                          const std::string& func, size_t line)
{
    llvm::sys::Path p = get_file_path(flag);
    std::ofstream ofs(p.c_str());

    assert(ofs.good());

    std::stringstream ss;

    ss << "[CRETE Exception] " << to_string(flag) << "\n"
       << "\tFunction: " << func << "\n"
       << "\tLine: " << line << "\n"
       << "\tMessage: " << msg << "\n";

    CRETE_DBG(std::cerr << ss.str(););

    ofs << ss.str();
}

void throw_exception_file(ExceptionFlag flag,
                          const klee::ref<klee::Expr> expr,
                          const klee::ConstraintManager& constraints,
                          const std::string& func, size_t line)
{
    std::stringstream ss;

    ss << "expression: ";
    klee::ExprPPrinter::printQuery(ss, constraints, expr);

    throw_exception_file(flag,
                         ss.str(),
                         func, line);
}

llvm::sys::Path get_file_path(ExceptionFlag flag)
{
    size_t id = 0;
    const char* parent = "exception";
    llvm::sys::Path p(parent, sizeof(parent) + 1);
    switch(flag)
    {
    case EXCEPTION_FLAG_SYMBOLIC_ADDR:
        p.appendComponent("symbolic-addr");
        static size_t sym_addr_id = 0;
        id = sym_addr_id++;
        break;
    default:
        assert(0);
    }

    bool existed = false;
    if(!fs::exists(p.c_str()))
        fs::create_directories(p.c_str(), existed);

    std::stringstream ss;
    ss << id;
    p.appendComponent(ss.str());

    return p;
}

std::string to_string(ExceptionFlag flag)
{
    switch(flag)
    {
    case EXCEPTION_FLAG_SYMBOLIC_ADDR:
        return "symbolic-address:";
        break;
    default:
        assert(0);
    }
}

void print_trace_tag(const crete::creteTraceTag_ty& trace_tag)
{
    for(crete::creteTraceTag_ty::const_iterator it = trace_tag.begin();
            it != trace_tag.end(); ++it) {
        fprintf(stderr, "tb-%lu: pc=%p, last_opc = %p",
                it->m_tb_count, (void *)it->m_tb_pc,
                (void *)(uint64_t)it->m_last_opc);
        fprintf(stderr, ", br_taken = ");
        crete::print_br_taken(it->m_br_taken);
        fprintf(stderr,"\n");
    }
}

} // namespace debug
} // namespace crete
