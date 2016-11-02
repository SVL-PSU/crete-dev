#ifndef CRETE_CLUSTER_DISPATCH_UI_H
#define CRETE_CLUSTER_DISPATCH_UI_H

#include <string>
#include <vector>
#include <deque>

#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/move/unique_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/property_tree/ptree.hpp>

#include <crete/cluster/dispatch.h>
#include <crete/cluster/dispatch_options.h>

namespace crete
{
namespace cluster
{

const auto default_master_port = crete::Port{10012};

class DispatchUI
{
public:
    DispatchUI(int argc, char* argv[]);
    ~DispatchUI();

    auto run() -> void;
    auto process_config(const boost::property_tree::ptree& config) -> option::Dispatch;

protected:
    auto parse_options(int argc, char* argv[]) -> void;
    auto make_options() -> boost::program_options::options_description;
    auto process_options() -> void;

private:
    boost::program_options::options_description ops_descr_;
    boost::program_options::variables_map var_map_;
    boost::movelib::unique_ptr<Dispatch> dispatch_;
    Port master_port_{0};
    option::Dispatch options_;
};

} // namespace cluster
} // namespace crete

#endif // CRETE_CLUSTER_DISPATCH_UI_H
