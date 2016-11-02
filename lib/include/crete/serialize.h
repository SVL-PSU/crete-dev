#ifndef CRETE_SERIALIZE_H
#define CRETE_SERIALIZE_H

#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

namespace crete
{
namespace serialize
{

template <typename OStream,
          typename T>
void write_text(OStream& os,
                const T& t)
{
    boost::archive::text_oarchive oa(os);
    oa << t;
}

template <typename IStream,
          typename T>
void read_text(IStream& is,
               T& t)
{
    boost::archive::text_iarchive ia(is);
    ia >> t;
}

} // namespace serialize
} // namespace crete

#endif // CRETE_SERIALIZE_H
