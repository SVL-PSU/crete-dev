#include "dispatch_ui.h"

#include <iomanip>
#include <iostream>
#include <limits>

#include <boost/program_options.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/move/make_unique.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <crete/exception.h>
#include <crete/cluster/node_driver.h>

namespace fs = boost::filesystem;
namespace po = boost::program_options;
namespace pt = boost::property_tree;

namespace crete
{
namespace cluster
{

DispatchUI::DispatchUI(int argc, char* argv[]) :
    ops_descr_(make_options())
{
    parse_options(argc, argv);
    process_options();
}

DispatchUI::~DispatchUI()
{
}

auto DispatchUI::run() -> void
{
    dispatch_ = boost::movelib::make_unique<Dispatch>(master_port_,
                                                      options_);

    bool running = true;

    while(running)
    {
        running = dispatch_->run();
    }
}

// TODO: this should be part of dispatch_options.h... as a ctor.
auto DispatchUI::process_config(const pt::ptree& config) -> option::Dispatch
{
    auto opts = option::Dispatch{};

    auto& crete = config.get_child("crete");

    opts.mode.name = crete.get<std::string>("mode");
    if(opts.mode.name == "distributed")
    {
        opts.mode.distributed = true;
    }
    else if(opts.mode.name == "developer")
    {
        opts.mode.distributed = false;
    }
    else
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::arg_invalid_str{opts.mode.name}
                                          << err::parse{"crete.mode"});
    }

    auto opt_trace = crete.get_child_optional("trace");
    if(opt_trace)
    {
        auto& trace = *opt_trace;

        opts.trace.filter_traces = trace.get<bool>("filter-traces", true);
        opts.trace.print_trace_selection = trace.get<bool>("print-trace-selection", false);
        opts.trace.print_graph = trace.get<bool>("print-graph", false);
        opts.trace.print_graph_only_branches = trace.get<bool>("print-graph-branches-only", false);
        opts.trace.print_elf_info = trace.get<bool>("print-elf-info", false);
        opts.trace.compress = trace.get<bool>("compress", false);

        if(opts.trace.print_graph && !opts.trace.filter_traces)
            throw Exception{} << err::parse{"trace.print-graph requires trace.filter-traces"};
        if(opts.trace.print_graph_only_branches)
            throw Exception{} << err::parse{"trace.print-graph-only-branches is no longer supported"};
    }

    auto opt_test = crete.get_child_optional("test");

    if(opt_test)
    {
        auto& test = *opt_test;

        opts.test.interval.trace = test.get<uint64_t>("interval.trace", std::numeric_limits<uint64_t>::max());
        opts.test.interval.tc = test.get<uint64_t>("interval.tc", std::numeric_limits<uint64_t>::max());
        opts.test.interval.time = test.get<uint64_t>("interval.time", std::numeric_limits<uint64_t>::max());

        if(opts.mode.distributed)
        {
            // auto-config mode: input the path to the output folder of crete-config-generator
            // It will take all the the guest-config.xml within that folder along with their
            // seed test cases.
            fs::path auto_config_folder = test.get<std::string>("auto-config-folder", "");

            if(fs::is_directory(auto_config_folder)) {
                std::string root_name = auto_config_folder.filename().string();

                std::vector<fs::path> v;
                std::copy(fs::directory_iterator(auto_config_folder),
                        fs::directory_iterator(), std::back_inserter(v));
                std::sort(v.begin(), v.end());
                for (std::vector<fs::path>::const_iterator it(v.begin()), it_end(v.end()); it != it_end; ++it)
                {
                    fs::path p_config_folder = *it;
                    fs::path p_config_file = p_config_folder / (p_config_folder.filename().string() + ".xml");
                    fs::path p_config_seeds = p_config_folder / "seeds";
//                    std::cerr << p_config_file.string() << ", " << p_config_seeds.string() << std::endl;
                    assert(fs::exists(p_config_file));
                    assert(fs::is_directory(p_config_seeds));

                    fs::path guest_path("/home/test/auto_configs");
                    guest_path = guest_path / root_name / p_config_folder.filename() / p_config_file.filename();
//                    std::cerr << guest_path.string() << ": " << p_config_seeds.string() << std::endl;
                    opts.test.items.emplace_back(guest_path.string());
                    opts.test.seeds.emplace_back(p_config_seeds.string());
                }
            } else {
                for(const auto& item : test.get_child("items"))
                {
                    if(item.first != "item")
                    {
                        BOOST_THROW_EXCEPTION(Exception{} << err::msg{"unexpected element"}
                        << err::parse{"crete.test.items.item"});
                    }

                    opts.test.items.emplace_back(item.second.get_value<std::string>());
                }

                if(opts.test.items.size() == 0)
                {
                    BOOST_THROW_EXCEPTION(Exception{} << err::arg_missing{"crete.test.items.item"}
                    << err::parse{"Must provide at least one item to test in '" + opts.mode.name + "' mode"});
                }
            }
        }
    }
    else
    {
        opts.test.interval.trace = std::numeric_limits<uint64_t>::max();
        opts.test.interval.tc = std::numeric_limits<uint64_t>::max();
        opts.test.interval.time = std::numeric_limits<uint64_t>::max();
    }


    {
        auto& vm = crete.get_child("vm");

        opts.vm.arch = vm.get<std::string>("arch");

        opts.vm.initial_tc = [&vm]
        {
            auto p = fs::path{vm.get<std::string>("initial-tc", "")};

            if(p.empty())
            {
                return TestCase{};
            }

            fs::ifstream ifs(p, std::ios::in | std::ios::binary);

            if(!ifs.good())
            {
                BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{p.string()});
            }

            return read_test_case(ifs);
        }();

        if(opts.mode.distributed)
        {
            opts.vm.image.path = vm.get<std::string>("image.path");
            opts.vm.image.update = vm.get<bool>("image.update", true);
            opts.vm.arch = vm.get<std::string>("arch");
            opts.vm.snapshot = vm.get<std::string>("snapshot");
            opts.vm.args = vm.get<std::string>("args", "");
        }
    }

    auto opt_svm = crete.get_child_optional("svm");
    if(opt_svm)
    {
        auto& svm = *opt_svm;

        opts.svm.args.concolic = svm.get<std::string>("args.concolic", "");
        opts.svm.args.symbolic = svm.get<std::string>("args.symbolic", "");
    }

    if(crete.get_child_optional("profile"))
    {
        auto const& profile = crete.get_child("profile");

        opts.profile.interval = profile.get<uint64_t>("interval", std::numeric_limits<uint32_t>::max());
    }

    auto elems = opts.test.items;
    for(auto& e : elems) { e = fs::path{e}.filename().string(); }
    std::sort(elems.begin(), elems.end());
    auto pos = std::adjacent_find(elems.begin(), elems.end());

    if(pos != elems.end())
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::msg{"duplicate items found: " + std::string{*pos}}
                                          << err::parse{"crete.test.items"});
    }

    return opts;
}

auto DispatchUI::parse_options(int argc, char* argv[]) -> void
{
    po::store(po::parse_command_line(argc, argv, ops_descr_), var_map_);
    po::notify(var_map_);
}

auto DispatchUI::make_options() -> po::options_description
{
    po::options_description desc("Options");

    desc.add_options()
            ("help,h", "displays help message")
            ("port,p", po::value<Port>(), "master port")
            ("config,c", po::value<fs::path>(), "[required] configuration file")
        ;

    return desc;
}

auto DispatchUI::process_options() -> void
{
    using namespace std;

    if(var_map_.size() == 0)
    {
        cout << "Missing arguments" << endl;
        cout << "Use '--help' for more details" << endl;

        BOOST_THROW_EXCEPTION(Exception{} << err::arg_missing{"required arguments not provided"});
    }
    if(var_map_.count("help"))
    {
        cout << ops_descr_ << endl;

        BOOST_THROW_EXCEPTION(Exception{});
    }
    if(var_map_.count("config"))
    {
        auto config_path = var_map_["config"].as<fs::path>();

        if(!fs::exists(config_path))
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::file_missing{config_path.string()});
        }

        fs::ifstream ifs(config_path);

        if(!ifs.good())
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{config_path.string()});
        }

        pt::ptree config;

        pt::read_xml(ifs,
                     config);

        options_ = process_config(config);
    }
    else
    {
        std::cout << "Missing --config argument" << std::endl;

        BOOST_THROW_EXCEPTION(Exception{});
    }
    if(var_map_.count("port"))
    {
        master_port_ = var_map_["port"].as<Port>();
    }
    else // Default port
    {
        master_port_ = default_master_port;
    }
}

} // namespace cluster
} // namespace crete

int main(int argc, char* argv[])
{
    using namespace std;
    using namespace crete;

    try
    {
        cluster::DispatchUI ui(argc, argv);

        ui.run();
    }
    catch(Exception& e)
    {
        cerr << "crete-dispatch: [CRETE] Exception: " << boost::diagnostic_information(e) << endl;
        return -1;
    }
    catch(std::exception& e)
    {
        cerr << "crete-dispatch: [std] Exception: " <<  boost::diagnostic_information(e) << endl;
        return -1;
    }
    catch(...)
    {
        cerr << "crete-dispatch: [...] Exception: " <<  boost::current_exception_diagnostic_information() << endl;
        return -1;
    }

    return 0;
}
