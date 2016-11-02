/***
 * Author: Christopher Havlicek
 ***/

#ifndef CRETE_NODE_REGISTRAR_H
#define CRETE_NODE_REGISTRAR_H

#include <vector>
#include <memory>

#include <boost/thread.hpp>

#include <crete/asio/server.h>
#include <crete/atomic_guard.h>
#include <crete/cluster/common.h>
#include <crete/dll.h>

namespace crete
{
namespace cluster
{

struct CRETE_DLL_EXPORT RegistrarNode
{
    RegistrarNode() // Giving 'Server' no port causes it to pick a valid one.
    {
    }

    Server server;
    NodeStatus status;
    uint32_t type = 0;
};

/**
 * @brief The NodeRegistrar class maintains a set of node connections and node statuses.
 */
class CRETE_DLL_EXPORT NodeRegistrar
{
public:
    using Node = std::shared_ptr<AtomicGuard<RegistrarNode>>;
    using Nodes = std::vector<Node>;

public:
    NodeRegistrar();

    auto open() const -> bool;
    auto open(bool o) -> void;
    auto nodes() -> Nodes&;
    auto disconnect() -> void;

private:
    bool registrar_open_ = true;
    Nodes nodes_;
};

/**
 * @brief The NodeRegistrarDriver class is used to continually check for new node registration
 * requests and notify the NodeRegistrar upon reception.
 */
class CRETE_DLL_EXPORT NodeRegistrarDriver
{
public:
    using Callback = std::function<void(NodeRegistrar::Node&)>;
    using Callbacks = std::vector<Callback>;

public:
    NodeRegistrarDriver(Port master_port,
                        AtomicGuard<NodeRegistrar>& registrar);
    NodeRegistrarDriver(Port master_port,
                        AtomicGuard<NodeRegistrar>& registrar,
                        const Callback& cb);

    auto run() -> void;
    auto connect() -> void;
    auto register_callback(const Callback& cb) -> void;
    auto call_back(NodeRegistrar::Node& node) -> void;

    auto operator()() -> void; // Invoked by thread. Calls run().

private:
    AtomicGuard<NodeRegistrar>& registrar_;
    Port master_port_; // Port through which node connections are established.
    bool shutdown_ = false;
    Callbacks callbacks_;
};

auto poll(NodeRegistrar::Node& node) -> NodeStatus;

} // namespace cluster
} // namespace crete

#endif // CRETE_NODE_REGISTRAR_H
