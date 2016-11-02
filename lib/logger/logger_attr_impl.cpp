#include "logger_attr_impl.h"

namespace crete
{
namespace log
{

StopWatchImpl::StopWatchImpl() :
    start_time_(boost::posix_time::microsec_clock::local_time())
{
}

std::string StopWatchImpl::print_elapsed() const
{
    boost::posix_time::time_duration diff = boost::posix_time::microsec_clock::local_time() - start_time_;

    return boost::posix_time::to_simple_string(diff);
}



} // namespace log
} // namespace crete
