#ifndef CRETE_CLUSTER_VM_NODE_UI_H
#define CRETE_CLUSTER_VM_NODE_UI_H

#include <string>
#include <vector>
#include <deque>

#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/move/unique_ptr.hpp>
#include <boost/thread.hpp>

#include <crete/cluster/vm_node.h>
#include <crete/cluster/vm_node_options.h>

namespace crete
{
namespace cluster
{

class VMNodeUI
{
public:
    VMNodeUI(int argc, char* argv[]);

    auto run() -> void;

protected:
    auto parse_options(int argc, char* argv[]) -> void;
    auto make_options() -> boost::program_options::options_description;
    auto process_options() -> void;

private:
    boost::program_options::options_description ops_descr_;
    boost::program_options::variables_map var_map_;
    node::option::VMNode node_options_;
};

} // namespace cluster
} // namespace crete

#endif // CRETE_CLUSTER_VM_NODE_UI_H
