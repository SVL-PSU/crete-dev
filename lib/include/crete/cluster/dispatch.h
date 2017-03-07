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
const auto dispatch_config_file_name = std::string{"dispatch_config.xml"};

const auto vm_test_multiplier = 1u;
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

} // namespace cluster
} // namespace crete

#endif // CRETE_CLUSTER_DISPATCH_H
