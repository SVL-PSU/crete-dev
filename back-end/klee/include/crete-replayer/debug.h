#ifndef CRETE_DEBUG_H
#define CRETE_DEBUG_H

#define CRETE_CROSS_CHECK

#define CRETE_DEBUG_GENERAL
//#define CRETE_DEBUG_MEMORY

//#define CRETE_DEBUG_XMM
//#define CRETE_DEBUG_FLOAT

//#define CRETE_DEBUG_TAINT_ANALYSIS

#define CRETE_DEBUG_TRACE_TAG

#define PRINT_TB_INDEX 0xfffffff

#ifdef CRETE_DEBUG_GENERAL
#define CRETE_DBG(x) do { x } while(0)
#else
#define CRETE_DBG(x) do { } while(0)
#endif

#ifdef CRETE_DEBUG_MEMORY
#define CRETE_DBG_MEMORY(x) do { x } while(0)
#else
#define CRETE_DBG_MEMORY(x) do { } while(0)
#endif

#ifdef CRETE_CROSS_CHECK
#define CRETE_CK(x) do { x } while(0)
#else
#define CRETE_CK(x) do { } while(0)
#endif

#ifdef CRETE_DEBUG_XMM
#define CRETE_DBG_XMM(x) do { x } while(0)
#else
#define CRETE_DBG_XMM(x) do { } while(0)
#endif

#ifdef CRETE_DEBUG_FLOAT
#define CRETE_DBG_FLT(x) do { x } while(0)
#else
#define CRETE_DBG_FLT(x) do { } while(0)
#endif

#ifdef CRETE_DEBUG_TAINT_ANALYSIS
#define CRETE_DBG_TA(x) do { x } while(0)
#else
#define CRETE_DBG_TA(x) do { } while(0)
#endif

#ifdef CRETE_DEBUG_TRACE_TAG
#define CRETE_DBG_TT(x) do { x } while(0)
#else
#define CRETE_DBG_TT(x) do { } while(0)
#endif

#include <crete/trace_tag.h>

#include <stdint.h>
#include <string>
#include <map>

#include "llvm/Support/Path.h"

namespace klee
{
//class MemoryManager;
//class AddressSpace;
class Expr;
//class ExecutionState;
class ConstraintManager;
template <typename T> class ref;
} // namespace klee

namespace crete
{
namespace debug
{

enum ExceptionFlag
{
    EXCEPTION_FLAG_SYMBOLIC_ADDR = 0
};

llvm::sys::Path get_file_path(ExceptionFlag flag);
std::string to_string(ExceptionFlag flag);

void throw_exception_file(ExceptionFlag flag,
                          const std::string& msg,
                          const std::string& func, size_t line);
void throw_exception_file(ExceptionFlag flag,
                          const klee::ref<klee::Expr> expr,
                          const klee::ConstraintManager& constraints,
                          const std::string& func, size_t line);

void print_trace_tag(const crete::creteTraceTag_ty& trace_tag);

} // namespace debug
} // namespace crete

#endif // CRETE_DEBUG_H
