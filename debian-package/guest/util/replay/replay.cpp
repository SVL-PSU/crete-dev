#include "replay.h"

#include <boost/filesystem/fstream.hpp>
#include <boost/program_options.hpp>

#include <string>
#include <ctime>

using namespace std;

static const string replay_log_file = "crete.replay.log";
static const string replay_current_tc = "crete.replay.current.tc.bin";
static const string replay_guest_config = "crete.replay.guest.config.serialized";

namespace crete
{

CreteReplay::CreteReplay(int argc, char* argv[]) :
    m_ops_descr(make_options()),
    m_cwd(fs::current_path())
{
    process_options(argc, argv);
    setup_launch();
    replay();
}

po::options_description CreteReplay::make_options()
{
    po::options_description desc("Options");

    desc.add_options()
        ("help,h", "displays help message")
        ("exec,e", po::value<fs::path>(), "executable to test")
        ("config,c", po::value<fs::path>(), "configuration file (found in guest-data/)")
        ("tc-dir,t", po::value<fs::path>(), "test case directory")
        ("seed-only,s", "Only replay seed test case (\"1\") from each test case "
                "directory")
        ;

    return desc;
}

void CreteReplay::process_options(int argc, char* argv[])
{
    try
    {
        po::store(po::parse_command_line(argc, argv, m_ops_descr), m_var_map);
        po::notify(m_var_map);
    }
    catch(...)
    {
        cerr << boost::current_exception_diagnostic_information() << endl;
        BOOST_THROW_EXCEPTION(std::runtime_error("Error for parsing options!\n"));
    }

    if(m_var_map.size() == 0)
    {
        cout << "Missing arguments" << endl;
        cout << "Use '--help' for more details" << endl;
        exit(0);
    }
    if(m_var_map.count("help"))
    {
        cout << m_ops_descr << endl;
        exit(0);
    }

    if(m_var_map.count("exec") && m_var_map.count("tc-dir") && m_var_map.count("config"))
    {
        m_exec = m_var_map["exec"].as<fs::path>();
        m_tc_dir = m_var_map["tc-dir"].as<fs::path>();
        m_config = m_var_map["config"].as<fs::path>();
    } else {
        BOOST_THROW_EXCEPTION(std::runtime_error("Required options: [exec] [tc-dir] [config]. See '--help' for more info"));
    }

    if(m_var_map.count("seed-only"))
    {
        m_seed_mode = true;
    } else {
        m_seed_mode = false;
    }

    if(!fs::exists(m_exec))
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Executable not found: "
                + m_exec.generic_string()));
    }
    if(!fs::exists(m_tc_dir))
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Input test case directory not found: "
                + m_tc_dir.generic_string()));
    }

    if(!fs::exists(m_config))
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Crete-config file not found: " + m_config.string()));
    }
}

void CreteReplay::setup_launch()
{
    // 0. Process m_config
    fs::ifstream ifs(m_config.string());

    if(!ifs.good())
    {
        BOOST_THROW_EXCEPTION(Exception() << err::file_open_failed(m_config.string()));
    }

    config::HarnessConfiguration guest_config;
    try
    {
        boost::archive::text_iarchive ia(ifs);
        ia >> guest_config;
    }
    catch(std::exception& e)
    {
        cerr << boost::diagnostic_information(e) << endl;
        BOOST_THROW_EXCEPTION(e);
    };

    // 1. Setup m_launch_directory
    m_launch_directory = m_exec.parent_path();

    // 2. Set up m_launch_args
    config::Arguments guest_args = guest_config.get_arguments();

    // FIXME: xxx assume argv[0] is not a part of guest_config
    m_launch_args.resize(guest_args.size()+1, string());
    m_launch_args[0] = m_exec.string();

    for(config::Arguments::const_iterator it = guest_args.begin();
            it != guest_args.end(); ++it) {
        assert(it->index < m_launch_args.size());
        assert(m_launch_args[it->index].empty());
        m_launch_args[it->index] = it->value;
    }

    // 3. Setup m_launch_ctx
    m_launch_ctx.stdout_behavior = bp::capture_stream();
    m_launch_ctx.stderr_behavior = bp::redirect_stream_to_stdout();
    m_launch_ctx.stdin_behavior = bp::capture_stream();
    m_launch_ctx.work_directory = m_launch_directory.string();
    m_launch_ctx.environment = bp::self::get_environment();
    m_launch_ctx.environment.erase("LD_PRELOAD");
    m_launch_ctx.environment.insert(bp::environment::value_type("LD_PRELOAD", "libcrete_replay_preload.so"));
}

// Get current date/time, format is YYYY-MM-DD.HH:mm:ss
static const std::string currentDateTime() {
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    // for more information about date/time format
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}

void CreteReplay::replay()
{
    // Read all test cases to replay
    vector<TestCase> tcs = retrieve_tests(m_tc_dir.string());
    fs::ofstream ofs_replay_log(m_cwd / replay_log_file, std::ios_base::app);

    ofs_replay_log << "Replay Summary: [" << currentDateTime() << "]\n"
            << "Executable path: " << m_exec.string() << endl
            << "Test case directory path: " << m_tc_dir.string() << endl
            << "Guest config path: " << m_config.string() << endl
            << "Working directory: " << m_cwd.string() << endl
            << "Launch direcotory: " << m_launch_directory.string() << endl
            << "Number of test cases: " << tcs.size() << endl
            << endl;

    uint64_t replayed_tc_count = 1;
    for(vector<TestCase>::const_iterator it = tcs.begin();
            it != tcs.end(); ++it) {
        if(m_seed_mode && (it != tcs.begin()))
        {
            break;
        }

        ofs_replay_log << "====================================================================\n";
        ofs_replay_log << "Start to replay tc-" << dec << replayed_tc_count++ << endl;

        // write replay_current_tc, for replay-preload to use
        {
            std::ofstream ofs((m_launch_directory / replay_current_tc).string().c_str());
            if(!ofs.good())
            {
                BOOST_THROW_EXCEPTION(Exception() << err::file_open_failed(m_config.string()));
            }
            it->write(ofs);
            ofs.close();
        }

        // copy guest-config, for replay-preload to use
        {
            try
            {
                fs::remove(m_launch_directory / replay_guest_config);
                fs::copy(m_config, m_launch_directory / replay_guest_config);
            }
            catch(std::exception& e)
            {
                cerr << boost::diagnostic_information(e) << endl;
                BOOST_THROW_EXCEPTION(e);
            }
        }

        // Launch the executable
        {
#if 0
            std::string exec_cmd = "LD_PRELOAD=\"libcrete_replay_preload.so\" ";
            for(vector<string>::const_iterator it = m_launch_args.begin();
                    it != m_launch_args.end(); ++it) {
                exec_cmd = exec_cmd + (*it) + " ";
            }

            std::cerr << "Launch program with system(): " << exec_cmd << std::endl;

            std::system(exec_cmd.c_str());
#else
            bp::child proc = bp::launch(m_exec, m_launch_args, m_launch_ctx);

            ofs_replay_log << "Output from Launched executable:\n";
            bp::pistream& is = proc.get_stdout();
            std::string line;
            while(getline(is, line))
            {
                ofs_replay_log << line << endl;
            }

            for( fs::directory_iterator dir_iter(m_launch_directory), end_iter ; dir_iter != end_iter ; ++dir_iter)
            {
                string filename = dir_iter->path().filename().string();
                if(filename.find("crete.replay.") == 0) {
                    fs::remove(m_launch_directory/filename);
                    ofs_replay_log << "Removed " << filename << endl;
                }
            }
#endif

//            auto status = proc.wait();
        }

        ofs_replay_log << "====================================================================\n";
    }
}

} // namespace crete

int main(int argc, char* argv[])
{
    try
    {
        crete::CreteReplay CreteReplay(argc, argv);
    }
    catch(...)
    {
        cerr << "[CRETE Replay] Exception Info: \n"
                << boost::current_exception_diagnostic_information() << endl;
        return -1;
    }

    return 0;
}
