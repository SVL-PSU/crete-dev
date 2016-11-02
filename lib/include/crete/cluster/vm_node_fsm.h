#ifndef CRETE_CLUSTER_VM_NODE_FSM_H
#define CRETE_CLUSTER_VM_NODE_FSM_H

#include <crete/cluster/node.h>
#include <crete/test_case.h>
#include <crete/asio/server.h>
#include <crete/dll.h>

#include <vector>
#include <memory>

#include <boost/msm/back/state_machine.hpp> // back-end

namespace crete
{
namespace cluster
{
namespace node
{
namespace vm
{

// +--------------------------------------------------+
// + Flags                                            +
// +--------------------------------------------------+
namespace flag
{
    struct guest_data_rxed {};
    struct trace_ready {};
    struct next_test {};
    struct error {};
    struct terminated {};
}

// +--------------------------------------------------+
// + Events                                           +
// +--------------------------------------------------+
namespace ev
{
    struct start;
    struct poll {};
    struct first;
    struct next_test;
    struct trace_queued {};
    struct terminate {};
}

namespace fsm
{

class QemuFSM;

} // namespace fsm

} // namespace node
} // namespace vm
} // namespace cluster
} // namespace crete

#endif // CRETE_CLUSTER_VM_NODE_FSM_H
