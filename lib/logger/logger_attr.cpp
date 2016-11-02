#include <crete/logger_attr.h>
#include <crete/logger.h>
#include "logger_attr_impl.h"


#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

using namespace std;

namespace crete
{
namespace log
{

Attribute::Attribute()
{

}

Attribute::Attribute(string title) :
    title_(title)
{
}

const string& Attribute::title() const
{
    return title_;
}

Stopwatch::Stopwatch() :
    impl_(boost::make_shared<StopWatchImpl>())
{
}

Stopwatch::Stopwatch(string title) :
    Attribute(title),
    impl_(boost::make_shared<StopWatchImpl>())
{

}

string Stopwatch::get_elapsed() const
{
    return impl_->print_elapsed();
}

string Stopwatch::print() const
{
    return impl_->print_elapsed();
}

ostream& operator<<(ostream& lhs, const Attribute& rhs)
{
    if(!rhs.title().empty())
        lhs << rhs.title() << ": " << rhs.print();
    else
        lhs << rhs.print();

    return lhs;
}

}
}
