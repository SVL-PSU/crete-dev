#ifndef CRETE_CLUSTER_NODE_H
#define CRETE_CLUSTER_NODE_H

#include <string>
#include <vector>
#include <deque>

#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/filesystem/path.hpp>

#include <crete/cluster/common.h>
#include <crete/cluster/dispatch_options.h>
#include <crete/test_case.h>
#include <crete/atomic_guard.h>
#include <crete/asio/common.h>
#include <crete/asio/client.h>
#include <crete/dll.h>

namespace crete
{
namespace cluster
{

auto generate_identifier() -> ID;

/**
 * @brief The Node class
 *
 * Invariants:
 * - ID and type remain unchanged for the lifetime of the object.
 * - trace and test queues are inserted and removed in FIFO fashion.
 */
class CRETE_DLL_EXPORT Node
{
public:
    using Type = uint16_t;
    using TraceQueue = std::deque<boost::filesystem::path>;
    using TestQueue = std::deque<TestCase>;
    using ErrorQueue = std::deque<log::NodeError>;
    using Tests = std::vector<TestCase>;
    using Traces = std::vector<boost::filesystem::path>; // TODO: using Trace = boost::filesystem::path once removed old Trace struct.

public:
    Node(const Type& t);
    Node(const ID& id, const Type& t);

    auto id() -> ID;
    auto status() const -> NodeStatus;
    auto push(const boost::filesystem::path& trace) -> void;
    auto push(const Traces& traces) -> void;
    auto push(const TestCase& tc) -> void;
    auto push(const Tests& tcs) -> void;
    auto push(const log::NodeError& e) -> void;
    auto pop_trace() -> boost::filesystem::path;
    auto pop_test() -> TestCase;
    auto next_test() -> const TestCase&;
    auto pop_error() -> log::NodeError;
    auto type() -> Type;
    auto commence() -> void;
    auto commenced() -> bool;
    auto reset() -> void;
    auto active(bool p) -> void;
    auto is_active() -> bool;
    auto master_options() const -> const option::Dispatch&;
    auto update(const option::Dispatch& options) -> void;

    auto traces() const -> const TraceQueue&;
    auto tests() const -> const TestQueue&;
    auto errors() const -> const ErrorQueue&;

    // TODO: fix the overlap between these and push(). These are more generic, but the others are more consistent. Make up your mind.
    template <typename Container>
    auto push_traces(const Container& container) -> void;
    template <typename Container>
    auto push_tests(const Container& container) -> void;

private:
    ID id_;
    TraceQueue traces_;
    TestQueue test_cases_;
    ErrorQueue errors_;
    Type type_;
    bool commenced_{false};
    bool active_{true};
    option::Dispatch master_options_;
};

template <typename Container>
auto Node::push_traces(const Container& container) -> void
{
    for(const auto& e : container) // Could use traces_.insert...
    {
        traces_.emplace_front(e);
    }
}

template <typename Container>
auto Node::push_tests(const Container& container) -> void
{
    for(const auto& e : container)
    {
        test_cases_.emplace_front(e);
    }
}

} // namespace cluster
} // namespace crete

#endif // CRETE_CLUSTER_NODE_H
