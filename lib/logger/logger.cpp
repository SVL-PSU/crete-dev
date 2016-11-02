#include <crete/logger.h>
#include "logger_impl.h"

#include <boost/filesystem/path.hpp>

#include <iostream> // testing
using namespace std; // testing

namespace crete
{
namespace log
{

Logger::Logger() :
    logger_impl_(new LoggerImpl)
{
}

Logger::~Logger()
{
}

void Logger::log(const std::string& msg)
{
    logger_impl_->log(msg);
}

void Logger::add_sink(const boost::filesystem::path& file)
{
    logger_impl_->add_sink(file);
}

void Logger::add_sink(ostream& os)
{
    logger_impl_->add_sink(os);
}

void Logger::flush_sinks()
{
    logger_impl_->flush_sinks();
}

void Logger::auto_flush(bool b)
{
    logger_impl_->auto_flush(b);
}

void Logger::enable(bool b)
{
    logger_impl_->enable(b);
}

bool Logger::is_enabled()
{
    return logger_impl_->is_enabled();
}

}
} // namespace crete
