#define BOOST_TEST_MODULE libcrete_guest_run top-level test suite

#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/range/irange.hpp>
#include <boost/filesystem/fstream.hpp>

#include <crete/cluster/vm_node.h>
#include <crete/test_case.h>

#include <crete/cluster/node_registrar.h>

#include "../runner.h"

namespace fs = boost::filesystem;

using namespace crete;
using namespace crete::cluster;


BOOST_AUTO_TEST_SUITE(crete_guest_run)

BOOST_AUTO_TEST_CASE(node_vm_comm_sequence)
{
    try
    {
    auto vm_dir = fs::path{"vm"} / "1";

    fs::remove_all(vm_dir);
    fs::remove_all(vm_dir / "trace");

    VMNode vm_node{1, 8585};

    auto vm_node_run_100x = [&vm_node]() {
        for(auto i : boost::irange(0, 100))
        {
            (void)i;

            vm_node.run();
        }
    };

    boost::thread runner_thread{[]() {
            char* argv[] = {"crete-run",
                           "--ip", "localhost",
                           "--port", "8585",
                           "--config", "dummy.config.xml"};
            int argc = sizeof(argv) / sizeof(argv[0]);

        Runner runner(argc, argv);
    }};

    // Set up.
    vm_node_run_100x();

    vm_node.push(TestCase{});

    vm_node_run_100x();

    BOOST_CHECK_EQUAL(vm_node.status().test_case_count,
                      0);

    // Create dummy trace_ready file
    {
        auto p = vm_dir / hostfile_dir_name / trace_ready_name;
        fs::ofstream ofs{p};
        BOOST_CHECK(fs::exists(p));
    }

    // Create dummy trace
    fs::create_directories(vm_dir / "trace" / "unnamed");

    vm_node_run_100x();

    vm_node.push(TestCase{});

    vm_node_run_100x();

    BOOST_CHECK_EQUAL(vm_node.status().test_case_count,
                      0);

    // Create dummy trace_ready file
    {
        auto p = vm_dir / hostfile_dir_name / trace_ready_name;
        fs::ofstream ofs{p};
        BOOST_CHECK(fs::exists(p));
    }

    // Create dummy trace
    fs::create_directories(vm_dir / "trace" / "unnamed");

    runner_thread.join();

    }
    catch(std::exception& e)
    {
        std::cerr << "Exception: " << boost::diagnostic_information(e) << std::endl;
    }
    catch(...)
    {
        std::cerr << "Exception: " << boost::current_exception_diagnostic_information() << std::endl;
    }
}

BOOST_AUTO_TEST_SUITE_END()
