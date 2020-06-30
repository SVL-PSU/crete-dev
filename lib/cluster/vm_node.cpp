#include <crete/cluster/vm_node.h>
#include <crete/exception.h>
#include <crete/process.h>
#include <crete/asio/server.h>
#include <crete/run_config.h>
#include <crete/serialize.h>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/range/irange.hpp>
#include <boost/thread.hpp>
#include <boost/property_tree/ptree.hpp>

#include <boost/process.hpp>

#include <memory>

#include "vm_node_fsm.cpp" // Unfortunate workaround to accommodate Boost.MSM.

namespace bp = boost::process;
namespace fs = boost::filesystem;
namespace pt = boost::property_tree;

namespace crete
{
namespace cluster
{

VMNode::VMNode(const node::option::VMNode& node_options,
               const fs::path& pwd)
    : Node{packet_type::cluster_request_vm_node}
    , node_options_{node_options}
    , pwd_{pwd}
{
    if(node_options_.vm.count < 1)
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::arg_invalid_uint(node_options_.vm.count));
    }

    init_image_info();
    add_instances(node_options_.vm.count);
}

auto VMNode::run() -> void
{
    if(!commenced())
    {
        return;
    }

    poll();
}

auto VMNode::poll() -> void
{
    using namespace node::vm;

    auto any_active = false;

    for(auto& vm : vms_)
    {
        if(vm->is_flag_active<flag::error>())
        {
            using node::vm::fsm::QemuFSM;

            push(vm->error());

            std::cerr << "pushing error!\n";

            auto pwd = vm->pwd();

            vm.reset(new QemuFSM{}); // TODO: may leak. Can I do vm = std::make_shared<QemuFSM>()?

            vm->start();

            ev::start start_ev{
                master_options(),
                        node_options_,
                        pwd,
                        image_path(),
                        false,
                        target_,
                        (traces().size() == 0)
            };

            vm->process_event(start_ev);
        }
        else if(vm->is_flag_active<flag::trace_ready>())
        {
            push(vm->trace());
            push_guest_data_post_exec(vm->get_guest_data_post_exec());

            vm->process_event(ev::trace_queued{});
        }        
        else if(vm->is_flag_active<flag::next_test>())
        {
            auto has_tests = !tests().empty();

            if(has_tests)
            {
                using boost::msm::back::HANDLED_TRUE;

                auto t = next_test();

                if(HANDLED_TRUE == vm->process_event(ev::next_test{t}))
                {
                    pop_test();
                }
            }
        }
        else if(vm->is_flag_active<flag::guest_data_rxed>())
        {
            guest_data_ = vm->guest_data();
            vm->process_event(ev::poll{});
        }
        else if(vm->is_flag_active<flag::terminated>())
        {
            // Do nothing.
        }
        else
        {
        	//zl3 debug info
        	//printf("entered poll else just poll\n");

            vm->process_event(ev::poll{});
        }

        any_active = any_active
                     || (!vm->is_flag_active<flag::next_test>()
                         && !vm->is_flag_active<flag::terminated>());
    }

    active(any_active);
}

auto VMNode::start_FSMs() -> void
{
    using namespace node::vm;

    auto vm_path = pwd_.string();

    if(master_options().mode.distributed)
    {
        vm_path = vm_inst_pwd.string();
    }

    ev::start start_ev{
        master_options(),
        node_options_,
        vm_path,
        image_path(),
        false,
        target_,
        (traces().size() == 0)
    };

    auto vm_num = 1u;

    for(auto& fsm : vms_)
    {
        fsm->start();

        auto s = start_ev;

        if(vm_num == 1)
        {
            s.first_vm_ = true;
        }

        if(master_options().mode.distributed)
        {
            s.vm_dir_ /= std::to_string(vm_num++);
        }

        fsm->process_event(s);
    }
}

auto VMNode::init_image_info() -> void
{
    auto infop = pwd_ / node_image_dir / image_info_name;

    if(fs::exists(infop))
    {

        {
            auto imagep = pwd_ / node_image_dir / image_name;

            if(!fs::exists(imagep))
            {
                fs::remove(infop);

                return;
            }
        }

        fs::ifstream ifs{infop};

        if(!ifs.good())
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{infop.string()});
        }

        serialize::read_text(ifs,
                             image_info_);
    }
}

auto VMNode::update(const ImageInfo& ii) -> void
{
	//zl3 debug info
	printf("entered vmnode update\n");

    image_info_ = ii;
    auto img_dir = pwd_ / node_image_dir;
    auto infop = img_dir / image_info_name;
    
    if(!fs::exists(img_dir))
    {
        fs::create_directories(img_dir);
    }
    
    fs::ofstream ofs{infop};

    if(!ofs.good())
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{infop.string()});
    }
    
    serialize::write_text(ofs,
                          image_info_);
}

auto VMNode::image_info() -> const ImageInfo&
{
    return image_info_;
}

auto VMNode::image_path() -> fs::path
{
    return pwd_ / node_image_dir / image_name;
}

auto VMNode::reset() -> void
{
    using namespace node::vm;

    std::cerr << "reset()" << std::endl;

    if(!commenced())
    {
        return;
    }

    auto count = vms_.size();

    for(auto& vm : vms_)
    {
        vm->process_event(ev::terminate{});
    }

    Node::reset();

    vms_.clear();

    add_instances(count);
}

auto VMNode::target(const std::string& target) -> void
{
    target_ = target;
}

auto VMNode::guest_data() -> const boost::optional<GuestData>&
{
    return guest_data_;
}

auto VMNode::reset_guest_data() -> void
{
    guest_data_ = boost::optional<GuestData>();
}

auto VMNode::push_guest_data_post_exec(const GuestDataPostExec& input) ->void
{
    guest_data_post_exec_.emplace_front(input);
}

auto VMNode::pop_guest_data_post_exec() -> const GuestDataPostExec
{
    assert(!guest_data_post_exec_.empty());
    auto ret = guest_data_post_exec_.back();
    guest_data_post_exec_.pop_back();

    return ret;
}

auto process(AtomicGuard<VMNode>& node,
             NodeRequest& request) -> bool
{
    switch(request.pkinfo_.type)
    {
    case packet_type::cluster_image_info_request:
    	printf("1\n");
        transmit_image_info(node,
                            request.client_);
        return true;
    case packet_type::cluster_image_info:
    	printf("2\n");
        receive_image_info(node,
                           request.sbuf_);
        return true;
    case packet_type::cluster_image:
    	printf("3\n");
        receive_image(node,
                      request.client_);
        return true;
    case packet_type::cluster_next_target:
    	printf("4\n");
        receive_target(node,
                       request.sbuf_);
        return true;
    case packet_type::cluster_commence:
    {
    	printf("5\n");
        auto lock = node.acquire();

        lock->start_FSMs(); // Once we have the options from Dispatch, we can start the VM instances.
        lock->commence();

        return true;
    }
    case packet_type::cluster_request_guest_data:
    {
    	printf("6\n");
        transmit_guest_data(node,
                            request.client_);

        return true;
    }
    case packet_type::cluster_request_guest_data_post_exec:
    	printf("7\n");
        transmit_guest_data_post_exec(node,
                                      request.client_);

        return true;
    }

    return false;
}

auto transmit_guest_data(AtomicGuard<VMNode>& node,
                         Client& client) -> void
{
    auto guest_data = decltype(node.acquire()->guest_data()){};
    auto pkinfo = PacketInfo{0, 0, 0};
    pkinfo.type = packet_type::cluster_tx_guest_data;

    while(!guest_data)
    {
        auto lock = node.acquire();

        pkinfo.id = lock->id();
        guest_data = lock->guest_data();
        lock->reset_guest_data();
    }

    write_serialized_binary(client,
                            pkinfo,
                            *guest_data);
}

auto transmit_guest_data_post_exec(AtomicGuard<VMNode>& node,
                         Client& client) -> void
{
    auto lock = node.acquire();

    auto pkinfo = PacketInfo{0, 0, 0};
    pkinfo.id = lock->id();
    pkinfo.type = packet_type::cluster_tx_guest_data_post_exec;

    GuestDataPostExec guest_data_post_exec = lock->pop_guest_data_post_exec();

    write_serialized_binary(client,
                            pkinfo,
                            guest_data_post_exec);
}

auto transmit_image_info(AtomicGuard<VMNode>& node,
                           Client& client) -> void
{
    auto pkinfo = PacketInfo{0,0,0};
    auto ii = ImageInfo{};

    {
        auto lock = node.acquire();

        pkinfo.id = lock->id();
        pkinfo.type = packet_type::cluster_image_info;

        ii = lock->image_info();
    }

    write_serialized_binary(client,
                            pkinfo,
                            ii);
}

auto receive_image_info(AtomicGuard<VMNode>& node,
                        boost::asio::streambuf& sbuf) -> void
{
    std::cout << "receive_image_info" << std::endl;
    auto ii = ImageInfo{};

    read_serialized_binary(sbuf,
                           ii);

    node.acquire()->update(ii);
}

auto receive_image(AtomicGuard<VMNode>& node,
                   Client& client) -> void
{
    auto image_path = node.acquire()->image_path();

    std::cout << "receive_image: " << image_path.string() << std::endl;

    if(!fs::exists(image_path.parent_path()))
    {
        fs::create_directories(image_path.parent_path());
    }

    if(fs::exists(image_path))
    {
        fs::remove(image_path);
    }

    fs::ofstream ofs{image_path};

    CRETE_EXCEPTION_ASSERT(ofs.good(), err::file_open_failed{image_path.string()});

    read(client,
         ofs);

    std::cout << "receive_image: success" << std::endl;
}

auto receive_target(AtomicGuard<VMNode>& node,
                    boost::asio::streambuf& sbuf) -> void
{
    auto target = std::string{};

    read_serialized_binary(sbuf,
                           target);

    node.acquire()->target(target);
}

auto VMNode::add_instance() -> void
{
    vms_.emplace_back(std::make_shared<node::vm::fsm::QemuFSM>());
}

auto VMNode::add_instances(size_t count) -> void
{
    for(const auto& i : boost::irange(size_t(0), count))
    {
        (void)i;

        add_instance();
    }
}

} // namespace cluster
} // namespace crete
