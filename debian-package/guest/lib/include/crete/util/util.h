#ifndef CRETE_UTIL_H
#define CRETE_UTIL_H

#include <string>

#include <crete/exception.h>

namespace crete
{ 
namespace util
{ // TODO: unsure of whether to make these inline, or a separate util library. And whether to make it C++11 or not.

inline
std::istream::pos_type stream_size(std::istream& is)
{
    std::istream::pos_type current = is.tellg();
    is.seekg(0, std::ios::end);
    std::istream::pos_type size = is.tellg();
    is.seekg(current, std::ios::beg);

    return size;
}

} // util
} // namespace crete

#endif // CRETE_DEBUG_UTIL_H
