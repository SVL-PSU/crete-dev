#ifndef CRETE_LOGGER_ATTR_IMPL_H
#define CRETE_LOGGER_ATTR_IMPL_H

#include <boost/date_time.hpp>

namespace crete
{
namespace log
{

class StopWatchImpl
{
public:
    StopWatchImpl();

    std::string print_elapsed() const;

private:
    boost::posix_time::ptime start_time_;
};

}
}

#endif // CRETE_LOGGER_ATTR_IMPL_H
