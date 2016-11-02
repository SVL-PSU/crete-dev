#define BOOST_TEST_MODULE libcrete_cluster top-level test suite

#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <crete/cluster/node_registrar.h>
#include <crete/cluster/node.h>
#include <crete/cluster/node_driver.h>
#include <crete/cluster/svm_node.h>
#include <crete/cluster/vm_node.h>
#include <crete/cluster/dispatch.h>

namespace fs = boost::filesystem;
namespace bui = boost::uuids;

const auto master_port = crete::Port{10012};

//crete::AtomicGuard<crete::cluster::NodeRegistrar> g_registrar;
//boost::thread g_reg_driver_thread{crete::cluster::NodeRegistrarDriver{master_port,
//                                                                      g_registrar}};
crete::cluster::Dispatch g_dispatch{master_port};

void disconnect_nodes()
{
    g_dispatch.node_registrar().acquire()->disconnect();
}

struct PosixGlobalContext
{
    PosixGlobalContext()
    {
    }
    ~PosixGlobalContext()
    {
        using namespace crete;

        Client client("localhost",
                      std::to_string(master_port));
        client.connect();
        client.write(0,
                     packet_type::cluster_shutdown); // NodeRegistrarDriver tells all nodes to disconnect.
    }
};

BOOST_GLOBAL_FIXTURE(PosixGlobalContext);

BOOST_AUTO_TEST_SUITE(crete_cluster)

BOOST_AUTO_TEST_CASE(open_connection_and_poll)
{
    using namespace crete;
    using namespace crete::cluster;

    auto& registrar = g_dispatch.node_registrar();

    AtomicGuard<SVMNode> svm_node{1};
    svm_node.acquire()->push(TestCase{});
    boost::thread svm_node_driver_thread{NodeDriver<SVMNode>{"localhost",
                                                             master_port,
                                                             svm_node}};

    boost::this_thread::sleep(boost::posix_time::seconds(1)); // Ensure connection is established before proceeding.

    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().size(), 1);

    AtomicGuard<SVMNode> vm_node{1};
    vm_node.acquire()->push(TestCase{});
    boost::thread vm_node_driver_thread{NodeDriver<SVMNode>{"localhost",
                                                            master_port,
                                                            vm_node}};

    boost::this_thread::sleep(boost::posix_time::seconds(1)); // Ensure connection is established before proceeding.

    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().size(), 2);

    registrar.acquire()->poll();

    {
        auto guard = registrar.acquire();
        auto& nodes = guard->nodes();
        auto it_svm = std::find_if(nodes.begin(),
                                   nodes.end(),
                                   [&](const std::shared_ptr<RegistrarNode>& n){ return n->status.id == svm_node.acquire()->id(); });
        auto it_vm = std::find_if(nodes.begin(),
                                  nodes.end(),
                                  [&](const std::shared_ptr<RegistrarNode>& n){ return n->status.id == vm_node.acquire()->id(); });

        BOOST_ASSERT(it_svm != nodes.end());
        BOOST_ASSERT(it_vm != nodes.end());

        BOOST_CHECK_EQUAL(svm_node.acquire()->traces().size(), (*it_svm)->status.trace_count);
        BOOST_CHECK_EQUAL(svm_node.acquire()->tests().size(), (*it_svm)->status.test_case_count);

        BOOST_CHECK_EQUAL(vm_node.acquire()->traces().size(), (*it_vm)->status.trace_count);
        BOOST_CHECK_EQUAL(vm_node.acquire()->tests().size(), (*it_vm)->status.test_case_count);
    }

    disconnect_nodes();

    svm_node_driver_thread.join();
    vm_node_driver_thread.join();
}

BOOST_AUTO_TEST_CASE(svm_node_consume_traces_multi_instance)
{
    using namespace crete;
    using namespace crete::cluster;

    auto& registrar = g_dispatch.node_registrar();

    AtomicGuard<SVMNode> svm_node{2};

    boost::thread svm_node_driver_thread{NodeDriver<SVMNode>{"localhost",
                                                             master_port,
                                                             svm_node}};

    boost::this_thread::sleep(boost::posix_time::seconds(1)); // Ensure connection is established before proceeding.

    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().size(), 1);

    {
        auto t0 = from_trace_file("/home/chris/crete/data/trace/tmp/runtime-dump-0");
        auto t1 = from_trace_file("/home/chris/crete/data/trace/tmp/runtime-dump-1");
        auto t2 = from_trace_file("/home/chris/crete/data/trace/tmp/runtime-dump-2");
        auto t3 = from_trace_file("/home/chris/crete/data/trace/tmp/runtime-dump-3");
        auto t4 = from_trace_file("/home/chris/crete/data/trace/tmp/runtime-dump-4");

        auto lock = registrar.acquire();

        auto& node = lock->nodes().at(0);

        auto pkinfo = PacketInfo{node->status.id,
                                 0, // set by write_serialized_binary
                                 packet_type::cluster_trace};

        write_serialized_binary(node->server, pkinfo, t0);
        write_serialized_binary(node->server, pkinfo, t1);
        write_serialized_binary(node->server, pkinfo, t2);
        write_serialized_binary(node->server, pkinfo, t3);
        write_serialized_binary(node->server, pkinfo, t4);
    }

    boost::this_thread::sleep(boost::posix_time::seconds(60)); // Ensure connection is established before proceeding.

    while(svm_node.acquire()->have_trace())
        ;

    std::cout << "disconnect_nodes" << std::endl;

    disconnect_nodes();

    svm_node_driver_thread.join();
}

BOOST_AUTO_TEST_CASE(vm_registrar_svm_interaction)
{
    using namespace crete;
    using namespace crete::cluster;

    auto& registrar = g_dispatch.node_registrar();
    auto vm_inst_port = Port{10014};
    auto vm_node_index = 0;
    auto svm_node_index = 1;

    BOOST_TEST_MESSAGE("***VMNode setup***");
    AtomicGuard<VMNode> vm_node{1,
                                vm_inst_port};


    boost::thread vm_node_driver_thread{NodeDriver<VMNode>{"localhost",
                                                            master_port,
                                                            vm_node}};

    boost::this_thread::sleep(boost::posix_time::milliseconds(100)); // Ensure connection is established before proceeding.


    BOOST_TEST_MESSAGE("***Dummy crete-run setup***");
    boost::thread vm_inst_thread{[&vm_inst_port]() {
        Client vm_inst_client{"localhost",
                              std::to_string(vm_inst_port)};

        vm_inst_client.connect();

        vm_inst_client.write(0,
                     packet_type::cluster_port_request);

        auto pkinfo = vm_inst_client.read();
        Client inst{"localhost",
                    std::to_string(pkinfo.id)
                   };

        inst.connect();

        boost::property_tree::ptree pt;
        read_serialized_binary(inst,
                               pt);

        while(true)
            ; // Continue, so inst remains a valid connection.
    }};

    boost::this_thread::sleep(boost::posix_time::milliseconds(100)); // Ensure connection is established before proceeding.

    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().size(),
                      1);


    BOOST_TEST_MESSAGE("***SVMNode setup***");
    AtomicGuard<SVMNode> svm_node{1};

    boost::thread svm_node_driver_thread{NodeDriver<SVMNode>{"localhost",
                                                             master_port,
                                                             svm_node}};


    boost::this_thread::sleep(boost::posix_time::milliseconds(100));

    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().size(),
                      2);

    vm_node.acquire()->push(from_trace_file("/home/chris/crete/data/trace/tmp/runtime-dump-0"));

    g_dispatch.run();

    BOOST_TEST_MESSAGE("***Waiting for SVM to generate TCs...***");

    boost::this_thread::sleep(boost::posix_time::milliseconds(15000));

    registrar.acquire()->poll(); // Update node statuses

    BOOST_CHECK_EQUAL(svm_node.acquire()->status().test_case_count,
                      5);
    BOOST_CHECK_EQUAL(svm_node.acquire()->status().trace_count,
                      0);
    BOOST_CHECK_EQUAL(vm_node.acquire()->status().test_case_count,
                      0);
    BOOST_CHECK_EQUAL(vm_node.acquire()->status().trace_count,
                      0);
    BOOST_CHECK_EQUAL(g_dispatch.test_pool().count_next(),
                      0);
    BOOST_CHECK_EQUAL(g_dispatch.test_pool().count_all(),
                      0);
    BOOST_CHECK_EQUAL(g_dispatch.trace_pool().count_next(),
                      0);
    BOOST_CHECK_EQUAL(g_dispatch.trace_pool().count_all(),
                      1);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(svm_node_index)->status.test_case_count,
                      svm_node.acquire()->status().test_case_count);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(svm_node_index)->status.trace_count,
                      svm_node.acquire()->status().trace_count);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(vm_node_index)->status.test_case_count,
                      vm_node.acquire()->status().test_case_count);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(vm_node_index)->status.trace_count,
                      vm_node.acquire()->status().trace_count);

    g_dispatch.run(); // Receive TC from svm_node
    registrar.acquire()->poll(); // Update node statuses
    boost::this_thread::sleep(boost::posix_time::milliseconds(10));

    BOOST_CHECK_EQUAL(svm_node.acquire()->status().test_case_count,
                      4);
    BOOST_CHECK_EQUAL(svm_node.acquire()->status().trace_count,
                      0);
    BOOST_CHECK_EQUAL(vm_node.acquire()->status().test_case_count,
                      0);
    BOOST_CHECK_EQUAL(vm_node.acquire()->status().trace_count,
                      0);
    BOOST_CHECK_EQUAL(g_dispatch.test_pool().count_next(),
                      1);
    BOOST_CHECK_EQUAL(g_dispatch.test_pool().count_all(),
                      1);
    BOOST_CHECK_EQUAL(g_dispatch.trace_pool().count_next(),
                      0);
    BOOST_CHECK_EQUAL(g_dispatch.trace_pool().count_all(),
                      1);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(svm_node_index)->status.test_case_count,
                      svm_node.acquire()->status().test_case_count);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(svm_node_index)->status.trace_count,
                      svm_node.acquire()->status().trace_count);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(vm_node_index)->status.test_case_count,
                      vm_node.acquire()->status().test_case_count);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(vm_node_index)->status.trace_count,
                      vm_node.acquire()->status().trace_count);

    g_dispatch.run(); // Send TC to vm_node && receive TC from svm_node
    registrar.acquire()->poll(); // Update node statuses
    boost::this_thread::sleep(boost::posix_time::milliseconds(10));

    BOOST_CHECK_EQUAL(svm_node.acquire()->status().test_case_count,
                      3);
    BOOST_CHECK_EQUAL(svm_node.acquire()->status().trace_count,
                      0);
    BOOST_CHECK_EQUAL(vm_node.acquire()->status().test_case_count,
                      0);
    BOOST_CHECK_EQUAL(vm_node.acquire()->status().trace_count,
                      0);
    BOOST_CHECK_EQUAL(g_dispatch.test_pool().count_next(),
                      1);
    BOOST_CHECK_EQUAL(g_dispatch.test_pool().count_all(),
                      2);
    BOOST_CHECK_EQUAL(g_dispatch.trace_pool().count_next(),
                      0);
    BOOST_CHECK_EQUAL(g_dispatch.trace_pool().count_all(),
                      1);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(svm_node_index)->status.test_case_count,
                      svm_node.acquire()->status().test_case_count);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(svm_node_index)->status.trace_count,
                      svm_node.acquire()->status().trace_count);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(vm_node_index)->status.test_case_count,
                      vm_node.acquire()->status().test_case_count);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(vm_node_index)->status.trace_count,
                      vm_node.acquire()->status().trace_count);

    g_dispatch.run(); // Send TC to vm_node && receive TC from svm_node
    registrar.acquire()->poll(); // Update node statuses
    boost::this_thread::sleep(boost::posix_time::milliseconds(10));

    BOOST_CHECK_EQUAL(svm_node.acquire()->status().test_case_count,
                      2);
    BOOST_CHECK_EQUAL(svm_node.acquire()->status().trace_count,
                      0);
    BOOST_CHECK_EQUAL(vm_node.acquire()->status().test_case_count,
                      1);  // Always N-1 b/c first is consumed to be executed.
    BOOST_CHECK_EQUAL(vm_node.acquire()->status().trace_count,
                      0);
    BOOST_CHECK_EQUAL(g_dispatch.test_pool().count_next(),
                      1);
    BOOST_CHECK_EQUAL(g_dispatch.test_pool().count_all(),
                      3);
    BOOST_CHECK_EQUAL(g_dispatch.trace_pool().count_next(),
                      0);
    BOOST_CHECK_EQUAL(g_dispatch.trace_pool().count_all(),
                      1);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(svm_node_index)->status.test_case_count,
                      svm_node.acquire()->status().test_case_count);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(svm_node_index)->status.trace_count,
                      svm_node.acquire()->status().trace_count);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(vm_node_index)->status.test_case_count,
                      vm_node.acquire()->status().test_case_count);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(vm_node_index)->status.trace_count,
                      vm_node.acquire()->status().trace_count);

    g_dispatch.run(); // Send TC to vm_node && receive TC from svm_node
    registrar.acquire()->poll(); // Update node statuses
    boost::this_thread::sleep(boost::posix_time::milliseconds(10));

    BOOST_CHECK_EQUAL(svm_node.acquire()->status().test_case_count,
                      1);
    BOOST_CHECK_EQUAL(svm_node.acquire()->status().trace_count,
                      0);
    BOOST_CHECK_EQUAL(vm_node.acquire()->status().test_case_count,
                      2);
    BOOST_CHECK_EQUAL(vm_node.acquire()->status().trace_count,
                      0);
    BOOST_CHECK_EQUAL(g_dispatch.test_pool().count_next(),
                      1);
    BOOST_CHECK_EQUAL(g_dispatch.test_pool().count_all(),
                      4);
    BOOST_CHECK_EQUAL(g_dispatch.trace_pool().count_next(),
                      0);
    BOOST_CHECK_EQUAL(g_dispatch.trace_pool().count_all(),
                      1);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(svm_node_index)->status.test_case_count,
                      svm_node.acquire()->status().test_case_count);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(svm_node_index)->status.trace_count,
                      svm_node.acquire()->status().trace_count);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(vm_node_index)->status.test_case_count,
                      vm_node.acquire()->status().test_case_count);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(vm_node_index)->status.trace_count,
                      vm_node.acquire()->status().trace_count);

    g_dispatch.run(); // Send TC to vm_node && receive TC from svm_node
    registrar.acquire()->poll(); // Update node statuses
    boost::this_thread::sleep(boost::posix_time::milliseconds(10));

    BOOST_CHECK_EQUAL(svm_node.acquire()->status().test_case_count,
                      0);
    BOOST_CHECK_EQUAL(svm_node.acquire()->status().trace_count,
                      0);
    BOOST_CHECK_EQUAL(vm_node.acquire()->status().test_case_count,
                      3);
    BOOST_CHECK_EQUAL(vm_node.acquire()->status().trace_count,
                      0);
    BOOST_CHECK_EQUAL(g_dispatch.test_pool().count_next(),
                      1);
    BOOST_CHECK_EQUAL(g_dispatch.test_pool().count_all(),
                      5);
    BOOST_CHECK_EQUAL(g_dispatch.trace_pool().count_next(),
                      0);
    BOOST_CHECK_EQUAL(g_dispatch.trace_pool().count_all(),
                      1);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(svm_node_index)->status.test_case_count,
                      svm_node.acquire()->status().test_case_count);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(svm_node_index)->status.trace_count,
                      svm_node.acquire()->status().trace_count);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(vm_node_index)->status.test_case_count,
                      vm_node.acquire()->status().test_case_count);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(vm_node_index)->status.trace_count,
                      vm_node.acquire()->status().trace_count);

    g_dispatch.run(); // Send TC to vm_node && receive TC from svm_node
    registrar.acquire()->poll(); // Update node statuses
    boost::this_thread::sleep(boost::posix_time::milliseconds(10));

    BOOST_CHECK_EQUAL(svm_node.acquire()->status().test_case_count,
                      0);
    BOOST_CHECK_EQUAL(svm_node.acquire()->status().trace_count,
                      0);
    BOOST_CHECK_EQUAL(vm_node.acquire()->status().test_case_count,
                      4);
    BOOST_CHECK_EQUAL(vm_node.acquire()->status().trace_count,
                      0);
    BOOST_CHECK_EQUAL(g_dispatch.test_pool().count_next(),
                      0);
    BOOST_CHECK_EQUAL(g_dispatch.test_pool().count_all(),
                      5);
    BOOST_CHECK_EQUAL(g_dispatch.trace_pool().count_next(),
                      0);
    BOOST_CHECK_EQUAL(g_dispatch.trace_pool().count_all(),
                      1);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(svm_node_index)->status.test_case_count,
                      svm_node.acquire()->status().test_case_count);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(svm_node_index)->status.trace_count,
                      svm_node.acquire()->status().trace_count);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(vm_node_index)->status.test_case_count,
                      vm_node.acquire()->status().test_case_count);
    BOOST_CHECK_EQUAL(registrar.acquire()->nodes().at(vm_node_index)->status.trace_count,
                      vm_node.acquire()->status().trace_count);

    disconnect_nodes();

//    vm_inst_thread.join();
    vm_node_driver_thread.join();
    svm_node_driver_thread.join();

}

BOOST_AUTO_TEST_SUITE_END()
