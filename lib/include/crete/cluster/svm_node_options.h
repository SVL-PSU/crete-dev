#ifndef CRETE_SVM_NODE_OPTIONS
#define CRETE_SVM_NODE_OPTIONS

#include <boost/property_tree/ptree.hpp>
#include <boost/thread/thread.hpp>

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

struct SVM
{
    SVM() = default;
    SVM(const boost::property_tree::ptree& tree);

    struct Path
    {
        std::string concolic;
        std::string symbolic;
    } path;
    uint32_t count{std::max(boost::thread::hardware_concurrency()
                           ,1u)}; // Default value given in ctor.
};

struct SVMNode
{
    SVMNode() = default;
    SVMNode(const boost::property_tree::ptree& tree);

    Master master;
    SVM svm;
};

} // namespace option
} // namespace node
} // namespace cluster
} // namespace crete

#endif // CRETE_SVM_NODE_OPTIONS
