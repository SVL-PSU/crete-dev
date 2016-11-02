#ifndef CRETE_CLUSTER_VM_NODE_H
#define CRETE_CLUSTER_VM_NODE_H

#include <crete/cluster/vm_node_fsm.h>
#include <crete/cluster/node.h>
#include <crete/cluster/dispatch_options.h>
#include <crete/cluster/vm_node_options.h>
#include <crete/cluster/common.h>
#include <crete/test_case.h>
#include <crete/asio/server.h>
#include <crete/dll.h>

#include <vector>
#include <memory>

#include <boost/msm/back/state_machine.hpp> // back-end
#include <boost/optional.hpp>

namespace crete
{
namespace cluster
{

const auto vm_inst_pwd = boost::filesystem::path{"vm"};
const auto node_image_dir = boost::filesystem::path{"image"};

class CRETE_DLL_EXPORT VMNode : public Node
{
public:
    using VM = std::shared_ptr<node::vm::fsm::QemuFSM>; // TODO: should be unique_ptr, shouldn't it?
    using VMs = std::vector<VM>;

public:
    VMNode(const node::option::VMNode& node_options,
           const boost::filesystem::path& pwd);

    using Node::update;

    auto run() -> void;
    auto add_instance() -> void;
    auto add_instances(size_t count) -> void;
    auto start_FSMs() -> void;
    auto init_image_info() -> void;
    auto update(const ImageInfo& ii) -> void;
    auto image_info() -> const ImageInfo&;
    auto image_path() -> boost::filesystem::path;
    auto reset() -> void;
    auto target(const std::string& target) -> void;
    auto guest_data() -> const boost::optional<GuestData>&;
    auto reset_guest_data() -> void;

    auto poll() -> void;

private:
    node::option::VMNode node_options_;
    boost::filesystem::path pwd_; // For non-distributed mode.
    VMs vms_;
    ImageInfo image_info_;
    std::string target_;
    boost::optional<GuestData> guest_data_;
};

auto process(AtomicGuard<VMNode>& node,
             NodeRequest& request) -> bool;
auto transmit_guest_data(AtomicGuard<VMNode>& node,
                         Client& client) -> void;
auto transmit_image_info(AtomicGuard<VMNode>& node,
                           Client& client) -> void;
auto receive_image_info(AtomicGuard<VMNode>& node,
                        boost::asio::streambuf& sbuf) -> void;
auto receive_image(AtomicGuard<VMNode>& node,
                   Client& client) -> void;
auto receive_target(AtomicGuard<VMNode>& node,
                    boost::asio::streambuf& sbuf) -> void;

} // namespace cluster
} // namespace crete

#endif // CRETE_CLUSTER_VM_NODE_H
