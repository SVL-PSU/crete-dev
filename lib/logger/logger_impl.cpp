#include "logger_impl.h"

#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/expressions/formatters/stream.hpp>
#include <boost/log/support/date_time.hpp>

#include <iomanip>

using namespace std;
namespace bl = boost::log;

namespace crete
{
namespace log
{

LoggerImpl::LoggerImpl()
{
    bl::add_common_attributes();
}

LoggerImpl::~LoggerImpl()
{
    // Bug appeared to be with auto_ptr destructor. Using boost::shared_ptr now, and the sink is flushed/written automatically now.
//    flush_sinks();
}

void LoggerImpl::add_sink(const boost::filesystem::path& file_path)
{
    namespace expr = bl::expressions;

    // These should be in free functions.
   FileSink sink = bl::add_file_log(file_path);
    file_sinks_.push_back(sink);

    sink->set_formatter
            (
                expr::stream
                    << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "[%Y-%m-%d %H:%M:%S]: ")
                    << expr::smessage
            );
}

void LoggerImpl::add_sink(ostream& os)
{
    OSSink sink = bl::add_console_log(os);
    os_sinks_.push_back(sink);
}

void LoggerImpl::flush_sinks()
{
    for(vector<FileSink>::iterator iter = file_sinks_.begin();
        iter != file_sinks_.end();
        ++iter)
    {
        (*iter)->flush();
    }
}

void LoggerImpl::auto_flush(bool b)
{
    for(vector<FileSink>::iterator iter = file_sinks_.begin();
        iter != file_sinks_.end();
        ++iter)
    {
        (*iter)->locked_backend()->auto_flush(b);
    }
}

void LoggerImpl::enable(bool b)
{
    bl::core::get()->set_logging_enabled(b);
}

bool LoggerImpl::is_enabled()
{
    return bl::core::get()->get_logging_enabled();
}

}
}
