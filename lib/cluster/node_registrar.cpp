#include <crete/cluster/node_registrar.h>

#include <iostream>

#include <boost/msm/back/state_machine.hpp> // back-end
#include <boost/msm/front/state_machine_def.hpp> //front-end

#include <crete/asio/common.h>

namespace crete
{
namespace cluster
{

NodeRegistrar::NodeRegistrar()
{
}

auto NodeRegistrar::open() const -> bool
{
    return registrar_open_;
}

auto NodeRegistrar::open(bool o) -> void
{
    registrar_open_ = o;
}

auto NodeRegistrar::nodes() -> NodeRegistrar::Nodes&
{
    return nodes_;
}

auto NodeRegistrar::disconnect() -> void
{
    for(const auto& node : nodes_)
    {
        auto lock = node->acquire();

        lock->server.write(lock->status.id,
                           packet_type::cluster_shutdown);
    }

    nodes_.clear();
}

NodeRegistrarDriver::NodeRegistrarDriver(Port master_port,
                                         AtomicGuard<NodeRegistrar>& registrar) :
    registrar_(registrar),
    master_port_(master_port)
{
}

NodeRegistrarDriver::NodeRegistrarDriver(Port master_port,
                                         AtomicGuard<NodeRegistrar>& registrar,
                                         const Callback& cb) :
    registrar_(registrar),
    master_port_(master_port)
{
    register_callback(cb);
}

auto NodeRegistrarDriver::operator()() -> void
{
    run();
}

auto NodeRegistrarDriver::run() -> void
{
    while(!shutdown_)
    {
        std::cout << "[CRETE] Awaiting connection on '"
                  << boost::asio::ip::host_name()
                  << "' on port '"
                  << master_port_
                  << "' ..."
                  << std::endl;

        connect();

        // The port may be in a TIME_WAIT state - waiting for TCP to relay back that it's
        // closed the socket on the other side as well. *Guessing* 1s will be sufficient.
        // Wait for the next connection attempt.
        boost::this_thread::sleep_for(boost::chrono::seconds{1});
    }
}

auto NodeRegistrarDriver::connect() -> void
{
    Server registrar_server{master_port_};

    registrar_server.open_connection_wait();

    auto pkinfo = registrar_server.read();
    if(pkinfo.type == packet_type::cluster_request_vm_node ||
       pkinfo.type == packet_type::cluster_request_svm_node)
    {
        auto node = std::make_shared<AtomicGuard<RegistrarNode>>();

        {
            auto lock = node->acquire();

            auto node_id = pkinfo.id;
            auto node_type = pkinfo.type;
            auto new_port = lock->server.port();

            registrar_server.write(new_port,
                                   packet_type::cluster_port);

            lock->server.open_connection_wait(); // TODO: (relevant anymore with new shutdown signal?): If it doesn't connect... Timeout. See examples for deadline_timer. Also, there's a quick and simple use of std::future, but, alas, clang++ and libstdc++4.6 don't agree. Need at least libstc++4.8 or libc++. open_connection_async can also be used as a timeout mechanism.

        //            if(/*server didn't connect*/)
        //                error... or move on;

            // Request status.
            lock->server.write(node_id,
                               packet_type::cluster_status_request);

            cluster::NodeStatus status;
            read_serialized_binary(lock->server,
                                   status,
                                   packet_type::cluster_status);

            lock->status = status;
            lock->type = node_type;

        }

        call_back(node);

        // call_back must happen before push_back, or race condition ensues.
        // 'node' is local until this call makes it externally visible.
        registrar_.acquire()->nodes().push_back(node);
    }
    else if(pkinfo.type == packet_type::cluster_shutdown)
    {
        shutdown_ = true;

        registrar_.acquire()->disconnect();

        return;
    }
    else
    {
        throw std::runtime_error("unknown request from node");
    }
}

auto NodeRegistrarDriver::register_callback(const Callback& cb) -> void
{
    callbacks_.emplace_back(cb);
}

auto NodeRegistrarDriver::call_back(NodeRegistrar::Node& node) -> void
{
    for(auto& cb : callbacks_)
    {
        cb(node);
    }
}

auto poll(NodeRegistrar::Node& node) -> NodeStatus
{
    auto lock = node->acquire();
    lock->server.write(lock->status.id, // id of the node. For sanity check on the client (superfluous).
                       packet_type::cluster_status_request);

    cluster::NodeStatus status;
    read_serialized_binary(lock->server,
                           status,
                           packet_type::cluster_status);

    lock->status = status;

    return status;
}

} // namespace cluster
} // namespace crete

