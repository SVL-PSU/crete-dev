#ifndef CRETE_LOG_STREAM_H
#define CRETE_LOG_STREAM_H

#include <sstream>

#include <boost/shared_ptr.hpp>

namespace crete
{
namespace log
{
    class Logger;

    /**
     * @brief Allows for convenient stream-like behavior for Logger.
     *
     * Note: some inefficient comprimises were made to support pre-C++11. That is, rvalue
     * returned from function may require copy-construction. Since stringstream is non-copiable,
     * it had to be made heap-based. Some overhead from boost::shared_ptr.
     */
    class LogStream
    {
    public:
        template <typename T>
        LogStream(Logger& logger, const T& msg);
        ~LogStream();

        template <typename T>
        LogStream& operator<<(const T& msg);

    private:
        Logger& logger_;
        boost::shared_ptr<std::stringstream> stream_;
    };

    template <typename T>
    LogStream::LogStream(Logger& logger, const T& msg) :
        logger_(logger),
        stream_(new std::stringstream())
    {
        *stream_ << msg;
    }

    template <typename T>
    LogStream& LogStream::operator<<(const T& msg)
    {
        *stream_ << msg;

        return *this;
    }
}
}

#endif // CRETE_LOG_STREAM_H
