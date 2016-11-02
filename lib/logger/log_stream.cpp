#include <crete/log_stream.h>
#include <crete/logger.h>

namespace crete
{
namespace log
{

LogStream::~LogStream()
{
    logger_.log(stream_->str());
}

}
} // namespace crete
