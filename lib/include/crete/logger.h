#ifndef CRETE_LOGGER_H
#define CRETE_LOGGER_H

#include <string>
#include <sstream>
#include <memory>

#include <boost/shared_ptr.hpp>

#include <crete/log_stream.h>
#include <crete/registry.h> // Included for user convenience.

namespace boost
{
    namespace filesystem
    {
        class path;
    }
}

namespace crete
{
namespace log
{
    class LoggerImpl;
    /**
     * @brief Logging facility for CRETE.
     *
     * The underlying logger should be thread safe (using *_mt version of logger).
     *
     * Note: special care was taken to avoid exposing underlying Boost.Log
     * because Boost.Log requires RTTI. Thus, tools using LLVM (which has RTTI disabled), like Klee,
     * will need to dynamically link. Care was also taken to be pre-C++11 compliant.
     *
     * Usage:
     * Cleanest way is to use stream operator<< directly on Logger instance.
     * Any value that operator<<(istream&,const T&) supports will work (istream operator used internally).
     * Overloading operator<<(LogStream&,const TargetClass&) is the cleanest way to log classes.
     */
    class Logger
    {
    public:
        Logger();
        ~Logger();

        void log(const std::string& msg);
        /**
         * Accepted File Formats:
         *  "name.log"
         *  "log/name_%3N.log"
         *
         * @brief add_sink
         * @param file
         */
        void add_sink(const boost::filesystem::path& file);
        void add_sink(std::ostream& os);
        //void add_sink(...asio socket...);
        void flush_sinks();
        void auto_flush(bool b);
        void enable(bool b);
        bool is_enabled();


        template <typename T>
        LogStream operator<<(const T& msg);

    protected:
        void initialize_log();

    private:
        boost::shared_ptr<LoggerImpl> logger_impl_;
    };

    template <typename T>
    LogStream Logger::operator<<(const T& msg)
    {
        return LogStream(*this, msg);
    }
}
}

#endif // CRETE_LOGGER_H
