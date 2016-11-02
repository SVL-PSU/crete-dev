/***
 * Author: Christopher Havlicek
 ***/

#ifndef CRETE_CLUSTER_DISPATCH_H
#define CRETE_CLUSTER_DISPATCH_H

#include <vector>
#include <memory>

#include <crete/cluster/common.h>
#include <crete/cluster/node_registrar.h>
#include <crete/dll.h>
#include <crete/atomic_guard.h>
#include <crete/test_case.h>
#include <crete/cluster/test_pool.h>
#include <crete/cluster/trace_pool.h>
#include <crete/cluster/dispatch_options.h>

namespace crete
{
namespace cluster
{

const auto dispatch_root_dir_name = std::string{"dispatch"};
const auto dispatch_trace_dir_name = std::string{"trace"};
const auto dispatch_test_case_dir_name = std::string{"test-case"};
const auto dispatch_profile_dir_name = std::string{"profile"};
const auto dispatch_guest_data_dir_name = std::string{"guest-data"};
const auto dispatch_guest_config_file_name = std::string{"crete-guest-config.serialized"};
const auto dispatch_log_finish_file_name = std::string{"finish.log"};
const auto dispatch_log_test_case_tree_file_name = std::string{"test_case_tree.log"};
const auto dispatch_log_vm_dir_name = std::string{"vm"};
const auto dispatch_log_svm_dir_name = std::string{"svm"};
const auto dispatch_node_error_log_file_name = std::string{"node_error.log"};
const auto dispatch_last_root_symlink = std::string{"last"};
const auto vm_test_multiplier = 20u;
const auto vm_trace_multiplier = 20u;

namespace vm
{
class NodeFSM;
} // namespace vm

namespace svm
{
class NodeFSM;
} // namespace svm

namespace fsm
{
class DispatchFSM;
} // namespace fsm

/**
 * @brief The Dispatch class is responsible for communication between the various nodes.
 *
 * Invariants:
 *  -
 */
class CRETE_DLL_EXPORT Dispatch
{
public:
    using DispatchFSM = std::shared_ptr<fsm::DispatchFSM>;

public:
    Dispatch(Port master,
             const option::Dispatch& options);

    auto run() -> bool;
    auto has_nodes() -> bool;

private:
    auto start_FSM(Port master,
                   const option::Dispatch& options) -> void;

private:
    DispatchFSM dispatch_fsm_;
};

auto filter_vm(const NodeRegistrar::Nodes& nodes) -> NodeRegistrar::Nodes;
auto filter_svm(const NodeRegistrar::Nodes& nodes) -> NodeRegistrar::Nodes;
auto sort_by_trace(NodeRegistrar::Nodes& nodes) -> void;
auto sort_by_test(NodeRegistrar::Nodes& nodes) -> void;
auto receive_trace(NodeRegistrar::Node& node,
                   const boost::filesystem::path& traces_dir) -> boost::filesystem::path;
auto receive_tests(NodeRegistrar::Node& node) -> std::vector<TestCase>;
auto receive_errors(NodeRegistrar::Node& node) -> std::vector<log::NodeError>;
auto receive_image_info(NodeRegistrar::Node& node) -> ImageInfo;
auto transmit_trace(NodeRegistrar::Node& node,
                    const boost::filesystem::path& traces) -> void;
auto transmit_tests(NodeRegistrar::Node& node,
                    const std::vector<TestCase>& tcs) -> void;
auto transmit_commencement(NodeRegistrar::Node& node) -> void;
auto transmit_image_info(NodeRegistrar::Node& node,
                         const ImageInfo& ii) -> void;
auto transmit_config(NodeRegistrar::Node& node,
                     const option::Dispatch& options) -> void;
auto register_node_fsm(NodeRegistrar::Node& node,
                       const option::Dispatch& options,
                       const boost::filesystem::path& root,
                       AtomicGuard<std::vector<std::shared_ptr<vm::NodeFSM>>>& vm_node_fsms,
                       AtomicGuard<std::vector<std::shared_ptr<svm::NodeFSM>>>& svm_node_fsms) -> void;
auto make_dispatch_root() -> boost::filesystem::path;
auto extract_initial_test(const config::RunConfiguration& config) -> TestCase;

} // namespace cluster
} // namespace crete

#endif // CRETE_CLUSTER_DISPATCH_H
