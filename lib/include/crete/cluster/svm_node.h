#ifndef CRETE_CLUSTER_SVM_NODE_H
#define CRETE_CLUSTER_SVM_NODE_H

#include <crete/cluster/node.h>
#include <crete/cluster/svm_node_fsm.h>
#include <crete/cluster/svm_node_options.h>
#include <crete/cluster/dispatch_options.h>
#include <crete/test_case.h>
#include <crete/atomic_guard.h>
#include <crete/dll.h>

#include <vector>
#include <memory>

#include <boost/thread.hpp>

namespace crete
{
namespace cluster
{

const auto svm_working_dir_name = std::string{"svm"};

class CRETE_DLL_EXPORT SVMNode : public Node
{
public:
    using SVM = std::shared_ptr<node::svm::fsm::KleeFSM>;
    using SVMs = std::vector<SVM>;

public:
    SVMNode(node::option::SVMNode node_options);

    auto run() -> void;
    auto start_FSMs() -> void;
    auto have_trace() const -> bool;
    auto add_instance() -> void;
    auto add_instances(size_t count) -> void;
    auto clean() -> void;
    auto reset() -> void;
    auto poll() -> void;
    auto traces_directory() const -> boost::filesystem::path;

private:
    SVMs svms_;
    node::option::SVMNode node_options_;
};

auto process(AtomicGuard<SVMNode>& node,
             NodeRequest& request) -> bool;
auto receive_trace(AtomicGuard<SVMNode>& node,
                   boost::asio::streambuf& sbuf,
                   Client& client) -> void;

} // namespace cluster
} // namespace crete

#endif // CRETE_CLUSTER_SVM_NODE_H
