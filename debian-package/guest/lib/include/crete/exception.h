/***
 * Author: Christopher Havlicek
 */

#ifndef CRETE_EXCEPTION_H
#define CRETE_EXCEPTION_H

#include <exception>
#include <stdexcept>
#include <string>

#include <boost/exception/all.hpp>
#include <boost/throw_exception.hpp>
#include <boost/filesystem.hpp>

#include <sys/types.h>

namespace crete
{

namespace err
{
    // General
    typedef boost::error_info<struct tag_msg, std::string> msg;
    typedef boost::error_info<struct tag_invalid, std::string> invalid;
    typedef boost::error_info<struct tag_c_errno, int> c_errno;
    // File
    typedef boost::error_info<struct tag_file, std::string> file;
    typedef boost::error_info<struct tag_file_missing, std::string> file_missing;
    typedef boost::error_info<struct tag_file_exists, std::string> file_exists;
    typedef boost::error_info<struct tag_file_open_failed, std::string> file_open_failed;
    typedef boost::error_info<struct tag_file_create, std::string> file_create;
    typedef boost::error_info<struct tag_file_remove, std::string> file_remove;
    typedef boost::error_info<struct tag_file_size_limit_exceeded, std::string> file_size_limit_exceeded;
    // Process
    typedef boost::error_info<struct tag_process, std::string> process;
    typedef boost::error_info<struct tag_process_exit_status, std::string> process_exit_status;
    typedef boost::error_info<struct tag_process_not_exited, std::string> process_not_exited;
    typedef boost::error_info<struct tag_process_exited, std::string> process_exited;
    typedef boost::error_info<struct tag_process_error, pid_t> process_error;
    // ???
    typedef boost::error_info<struct tag_obj_not_available, std::string> obj_not_available;
    // Arguments
    typedef boost::error_info<struct tag_arg_invalid_uint, size_t> arg_invalid_uint;
    typedef boost::error_info<struct tag_arg_invalid_str, std::string> arg_invalid_str;
    typedef boost::error_info<struct tag_arg_count, size_t> arg_count;
    typedef boost::error_info<struct tag_arg_missing, std::string> arg_missing;
    // Environment
    typedef boost::error_info<struct tag_invalid_env, std::string> invalid_env;
    // Parsing
    typedef boost::error_info<struct tag_parse, std::string> parse;
    // Network
    typedef boost::error_info<struct tag_network, std::string> network;
    typedef boost::error_info<struct tag_network_type, uint32_t> network_type;
    typedef boost::error_info<struct tag_network_mismatch, uint32_t> network_type_mismatch; // TODO: should include uint32_t,uint32_t to show the mismatch.
    // Mode
    typedef boost::error_info<struct tag_mode, std::string> mode;
    // Misc.
    typedef boost::error_info<struct tag_stream_size, int64_t> stream_size;
    typedef boost::error_info<struct tag_unsigned_to_signed_conversion, int64_t> unsigned_to_signed_conversion;
    typedef boost::error_info<struct tag_signed_to_unsigned_conversion, int64_t> signed_to_unsigned_conversion;
    typedef boost::error_info<struct tag_unknown_exception, std::string> unknown_exception;
} // namespace err

struct Exception : virtual boost::exception, virtual std::exception {};

#define CRETE_EXCEPTION_ASSERT(pred, tag) if(!(pred)) { BOOST_THROW_EXCEPTION(crete::Exception() << (tag)); }
#define CRETE_EXCEPTION_ASSERT_X(pred, exception, tag) if(!(pred)) { BOOST_THROW_EXCEPTION((exception) << (tag)); }
#define CRETE_EXCEPTION_THROW(tag) BOOST_THROW_EXCEPTION(crete::Exception() << (tag))
#define CRETE_EXCEPTION_THROW_X(exception, tag) BOOST_THROW_EXCEPTION((exception) << (tag))

} // namespace crete

#endif // CRETE_EXCEPTION_H
