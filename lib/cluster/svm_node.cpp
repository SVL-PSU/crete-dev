#include <crete/cluster/svm_node.h>
#include <crete/exception.h>
#include <crete/cluster/common.h>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/range/irange.hpp>

#include <boost/process.hpp>

#include <crete/cluster/svm_node_fsm.h>
#include "svm_node_fsm.cpp" // Unfortunate workaround to accommodate Boost.MSM.

namespace bp = boost::process;
namespace fs = boost::filesystem;
namespace bui = boost::uuids;

namespace crete
{
namespace cluster
{

SVMNode::SVMNode(node::option::SVMNode node_options)
    : Node(packet_type::cluster_request_svm_node)
    , node_options_(node_options)

{
    if(node_options_.svm.count < 1)
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::arg_invalid_uint(node_options_.svm.count));
    }

    clean();

    add_instances(node_options_.svm.count);
}

auto SVMNode::run() -> void
{
    if(!commenced())
    {
        return;
    }

    poll();
}

auto SVMNode::poll() -> void
{
    using namespace node::svm;

    auto any_active = false;

    for(auto& svm : svms_)
    {
        if(svm->is_flag_active<flag::error>())
        {
            using node::svm::fsm::KleeFSM;

            push(svm->error());
            push(svm->tests());

            svm.reset(new KleeFSM{}); // TODO: may leak. Can I do vm = std::make_shared<KleeFSM>()?

            svm->start();

            svm->process_event(ev::start{svm_working_dir_name
                                        ,master_options()
                                        ,node_options_});
        }
        else if(svm->is_flag_active<flag::next_trace>())
        {
            if(have_trace())
            {
                auto t = pop_trace();
                svm->process_event(ev::next_trace{t});

//                std::ofstream ofs{"trace-seq.txt", std::ios::app};
//                ofs << t.filename().string() << "\n";
            }
        }
        else if(svm->is_flag_active<flag::tests_ready>())
        {
            push(svm->tests());

            svm->process_event(ev::tests_queued{});
        }
        else
        {
            svm->process_event(ev::poll{});
        }

        any_active = any_active || svm->is_flag_active<flag::active>();
    }

    active(any_active);
}

auto SVMNode::traces_directory() const -> fs::path
{
    return fs::path{svm_working_dir_name};
}

auto SVMNode::start_FSMs() -> void
{
    using namespace node::svm;

    for(auto& svm : svms_)
    {
        svm->start();

        svm->process_event(ev::start{svm_working_dir_name
                                    ,master_options()
                                    ,node_options_});
    }
}

auto SVMNode::have_trace() const -> bool
{
    return traces().size() > 0;
}

auto SVMNode::add_instance() -> void
{
    svms_.emplace_back(std::make_shared<node::svm::fsm::KleeFSM>());
}

auto SVMNode::add_instances(size_t count) -> void
{
    for(const auto& i : boost::irange(size_t(0), count))
    {
        (void)i;

        add_instance();
    }
}

auto SVMNode::clean() -> void
{
    fs::remove_all(svm_working_dir_name);
}


auto SVMNode::reset() -> void
{
    using namespace node::svm;

    std::cerr << "reset()" << std::endl;

    if(!commenced())
    {
        return;
    }

    auto count = svms_.size();

    for(auto& svm : svms_)
    {
        svm->process_event(ev::terminate{});
    }

    Node::reset();

    svms_.clear();

    add_instances(count);
}

auto process(AtomicGuard<SVMNode>& node,
             NodeRequest& request) -> bool
{
    switch(request.pkinfo_.type)
    {
    case packet_type::cluster_commence:
    {
        auto lock = node.acquire();

        lock->start_FSMs(); // Once we have the options from Dispatch, we can start the VM instances.
        lock->commence();

        return true;
    }
    case packet_type::cluster_trace:
        std::cout << "cluster_trace" << std::endl;
        receive_trace(node,
                      request.sbuf_,
                      request.client_);
        return true;
    }

    return false;
}

auto receive_trace(AtomicGuard<SVMNode>& node,
                   boost::asio::streambuf& sbuf,
                   Client& client) -> void
{
    node.acquire()->active(true);

    auto trace_name = std::string{};

    read_serialized_binary(sbuf,
                           trace_name);

    auto trace = node.acquire()->traces_directory() / trace_name;

    {
        fs::ofstream ofs{trace,
                         std::ios::out | std::ios::binary};

        CRETE_EXCEPTION_ASSERT(ofs.good(),
                               err::file_open_failed{trace.string()});

        read(client,
             ofs);
    }

    try
    {
        restore_directory(trace);
    }
    catch(std::exception& e)
    {
        std::cerr << boost::diagnostic_information(e) << std::endl;
        assert(0 && "exception raised while restoring trace"); // TODO: properly propagate exception.
    }

    node.acquire()->push(trace);
}

} // namespace cluster
} // namespace crete
