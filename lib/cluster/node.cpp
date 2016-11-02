#include <crete/cluster/node.h>

#include <iostream>
#include <chrono>

#include <crete/exception.h>

namespace fs = boost::filesystem;

namespace crete
{
namespace cluster
{

Node::Node(const Type& t) :
    Node(generate_identifier(),
         t)
{
}

Node::Node(const ID& id,
           const Node::Type& t) :
    id_(id),
    type_(t)
{

}

auto Node::id() -> ID
{
    return id_;
}

auto Node::status() const -> NodeStatus
{
    NodeStatus status;

    status.id = id_;
    status.test_case_count = test_cases_.size();
    status.trace_count = traces_.size();
    status.error_count = errors_.size();
    status.active = active_;

    return status;
}

auto Node::push(const fs::path& trace) -> void
{
    traces_.emplace_front(trace);
}

auto Node::push(const Traces& traces) -> void
{
    for(const auto& trace : traces)
    {
        traces_.emplace_front(trace);
    }
}

auto Node::push(const TestCase& tc) -> void
{
    test_cases_.emplace_front(tc);
}

auto Node::push(const Tests& tcs) -> void
{
    for(const auto& tc : tcs)
    {
        test_cases_.emplace_front(tc);
    }
}

auto Node::push(const log::NodeError& e) -> void
{
    errors_.emplace_front(e);
}

// TODO: technically, I think this is not exception-safe. 'pop_back' may throw. How is recovery in that case?
auto Node::pop_trace() -> fs::path
{
    assert(!traces_.empty());

    auto trace = traces_.back();

    traces_.pop_back();

    return trace;
}

auto Node::pop_test() -> TestCase
{
    assert(!test_cases_.empty());

    auto tc = test_cases_.back();

    test_cases_.pop_back();

    return tc;
}

auto Node::pop_error() -> log::NodeError
{
    assert(!errors_.empty());

    auto e = errors_.back();

    errors_.pop_back();

    return e;
}

auto Node::type() -> Type
{
    return type_;
}

auto Node::commence() -> void
{
    commenced_ = true;
}

auto Node::commenced() -> bool
{
    return commenced_;
}

void Node::reset()
{
    commenced_ = false;
    traces_.clear();
    test_cases_.clear();
    active_ = true;
}

auto Node::active(bool p) -> void
{
    active_ = p;
}

auto Node::is_active() -> bool
{
    return active_;
}

auto Node::master_options() const -> const option::Dispatch&
{
    return master_options_;
}

auto Node::update(const option::Dispatch& options) -> void
{
    master_options_ = options;
}

auto Node::traces() const -> const TraceQueue&
{
    return traces_;
}

auto Node::tests() const -> const TestQueue&
{
    return test_cases_;
}

auto Node::errors() const -> const ErrorQueue&
{
    return errors_;
}

auto generate_identifier() -> ID
{
    // TODO: use boost::uuids instead. Safer.

    auto duration = std::chrono::system_clock::now().time_since_epoch();

    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

} // namespace cluster
} // namespace crete
