#ifndef CRETE_LLVM_PARSER_H
#define CRETE_LLVM_PARSER_H

#include <crete/trace.h>

#include <boost/filesystem/path.hpp>

namespace crete
{
    Trace parse_trace(const boost::filesystem::path& path);
}

#endif // CRETE_LLVM_PARSER_H
