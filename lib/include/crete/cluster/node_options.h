#ifndef CRETE_NODE_OPTIONS
#define CRETE_NODE_OPTIONS

#include <boost/property_tree/ptree.hpp>

#include <string>
#include <stdint.h>

#include <crete/asio/common.h>

namespace crete
{
namespace cluster
{
namespace node
{
namespace option
{

const auto default_ip_addresss = std::string{"localhost"};
const auto default_master_port = crete::Port{10012};

struct Master
{
    Master() = default;
    Master(const boost::property_tree::ptree& tree);

    std::string ip{default_ip_addresss};
    Port port{default_master_port};
};

} // namespace option
} // namespace node
} // namespace cluster
} // namespace crete

#endif // CRETE_NODE_OPTIONS
