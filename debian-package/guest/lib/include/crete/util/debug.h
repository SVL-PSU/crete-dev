#ifndef CRETE_UTIL_DEBUG_H
#define CRETE_UTIL_DEBUG_H

#include <string>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/regex.hpp>

#include <crete/exception.h>

namespace crete
{ 
namespace util
{
namespace debug
{ // TODO: unsure of whether to make these inline, or a separate util library. And whether to make it C++11 or not.

inline
std::string get_last_line(const boost::filesystem::path& file) // TODO: should take an istream&?
{
    boost::filesystem::ifstream ifs(file);

    if(!ifs)
    {
        BOOST_THROW_EXCEPTION(Exception() << err::file_open_failed(file.string()));
    }

    ifs.seekg(-2, std::ios::end);
    char c;
    do
    {
        if(ifs.tellg() < 2)
            break;

        ifs.get(c);
        ifs.seekg(-2, std::ios::cur);
    }while(c != '\n');

    ifs.seekg(2, std::ios::cur);

    std::string line;
    getline(ifs, line);

    return line;
}

inline
bool is_last_line_correct(const boost::filesystem::path& file) // TODO: should take an istream&?
{
    auto line = get_last_line(file);

    return boost::regex_match(line, boost::regex("(KLEE: done: generated tests =)(.)*"));
}

} // namespace debug
} // util
} // namespace crete

#endif // CRETE_DEBUG_UTIL_H
