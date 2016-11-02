#ifndef CRETE_VM_NODE_OPTIONS
#define CRETE_VM_NODE_OPTIONS

#include <boost/property_tree/ptree.hpp>

#include <string>
#include <stdint.h>

#include <crete/cluster/node_options.h>

namespace crete
{
namespace cluster
{
namespace node
{
namespace option
{

struct VM
{
    VM() = default;
    VM(const boost::property_tree::ptree& tree);

    struct Path
    {
        std::string x86;
        std::string x64;
    } path;
    uint32_t count{1};
};

struct VMNode
{
    VMNode() = default;
    VMNode(const boost::property_tree::ptree& tree);

    Master master;
    VM vm;
};

} // namespace option
} // namespace node
} // namespace cluster
} // namespace crete

#endif // CRETE_VM_NODE_OPTIONS
