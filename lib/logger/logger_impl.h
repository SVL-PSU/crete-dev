#ifndef CRETE_LOGGER_IMPL_H
#define CRETE_LOGGER_IMPL_H

#include <string>
#include <vector>
#include <set>

#include <boost/log/sources/logger.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/sinks.hpp>
//#include <boost/log/attributes/timer.hpp>

#include <crete/logger_attr.h>

namespace crete
{
namespace log
{
    class LoggerImpl
    {
    public:
        typedef boost::shared_ptr<boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend> > FileSink;
        typedef boost::shared_ptr<boost::log::sinks::synchronous_sink<boost::log::sinks::basic_text_ostream_backend<char> > > OSSink;

    public:
        LoggerImpl();
        ~LoggerImpl();

        void add_sink(const boost::filesystem::path& file_path);
        void add_sink(std::ostream& os);
        void flush_sinks();
        void auto_flush(bool b);
        void enable(bool b);
        bool is_enabled();

        template <typename T>
        void log(const T& msg);

    protected:

    private:
        boost::log::sources::logger_mt logger_;
        std::vector<FileSink> file_sinks_;
        std::vector<OSSink> os_sinks_;
    };

    template <typename T>
    void LoggerImpl::log(const T& msg)
    {
        BOOST_LOG(logger_) << msg;
    }
}
}
#endif // CRETE_LOGGER_IMPL_H
