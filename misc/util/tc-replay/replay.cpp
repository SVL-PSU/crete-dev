#include "replay.h"
#include <crete/common.h>

#include <boost/filesystem/fstream.hpp>
#include <boost/program_options.hpp>

#include <string>
#include <ctime>
#include <sys/mount.h>

using namespace std;

static const string replay_log_file = "crete.replay.log";

namespace crete
{

CreteReplay::CreteReplay(int argc, char* argv[]) :
    m_ops_descr(make_options()),
    m_cwd(fs::current_path()),
    m_init_sandbox(true)
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
        ("input-sandbox,j", po::value<fs::path>(), "input sandbox/jail directory")
        ("no-ini-sandbox,n", po::bool_switch(), "do not initialize sandbox to accumulate coverage info")
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

    if(m_var_map.count("input-sandbox"))
    {
        fs::path p = m_var_map["input-sandbox"].as<fs::path>();

        if(!fs::exists(p) && !fs::is_directory(p))
        {
            BOOST_THROW_EXCEPTION(Exception() << err::file_missing(p.string()));
        }

        m_input_sandbox = p;

        if(m_var_map.count("no-ini-sandbox"))
        {
            bool input = m_var_map["no-ini-sandbox"].as<bool>();

            m_init_sandbox = !input;
        }

        fprintf(stderr, "[crete-replay] input_sandbox_dir = %s, m_init_sandbox = %d\n",
                m_input_sandbox.string().c_str(), m_init_sandbox);

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

// Reference:
// http://unix.stackexchange.com/questions/128336/why-doesnt-mount-respect-the-read-only-option-for-bind-mounts
static inline void rdonly_bind_mount(const fs::path src, const fs::path dst)
{
    assert(fs::is_directory(src));
    assert(fs::is_directory(dst));

    int mount_result = mount(src.string().c_str(), dst.string().c_str(), NULL,
            MS_BIND, NULL);
    if(mount_result != 0)
    {
        fprintf(stderr, "[crete-run] mount failed: "
                "src = %s, dst = %s, mntflags = MS_BIND\n",
                src.string().c_str(), dst.string().c_str());

        assert(0);
    }

    // equal cmd: "sudo mount /home sandbox-dir/home/ -o bind,remount,ro"
    mount_result = mount(src.string().c_str(), dst.string().c_str(), NULL,
            MS_BIND | MS_REMOUNT | MS_RDONLY, NULL);
    if(mount_result != 0)
    {
        fprintf(stderr, "[crete-run] mount failed: "
                "src = %s, dst = %s, mntflags = MS_BIND | MS_REMOUNT | MS_RDONLY\n",
                src.string().c_str(), dst.string().c_str());

        assert(0);
    }
}

static void reset_folder_permission_recursively(const fs::path& root)
{
    for(fs::recursive_directory_iterator it(root), endit;
            it != endit; ++it) {
        if(!fs::is_symlink(*it)){
            fs::permissions(*it, fs::owner_all);
        }
    }
}

// make sure the folder has the right permission within sandbox:
// 1. "/": the root of sandbox
// 2. "/tmp"
// 3. "/tmp/launch-directory"
void CreteReplay::reset_sandbox_folder_permission()
{
    {
        fs::path p = CRETE_SANDBOX_PATH;
        if(fs::exists(p))
        {
            fs::permissions(p, fs::perms_mask);
        }
    }

    {
        fs::path p = fs::path(CRETE_SANDBOX_PATH) / "tmp";
        if(fs::exists(p))
        {
            fs::permissions(p, fs::perms_mask);
        }
    }

    {
        fs::path p = fs::path(CRETE_SANDBOX_PATH) / m_launch_directory;
        if(fs::exists(p))
        {
            fs::permissions(p, fs::perms_mask);
            reset_folder_permission_recursively(p);
        }
    }
}

// Mount folders to sandbox dir:
//  "/home, /lib, /lib64, /usr, /dev, /proc" (for executable, dependency libraries, etc)
// require: "sudo setcap CAP_SYS_ADMIN+ep ./crete-run"
void CreteReplay::init_sandbox()
{
    reset_sandbox_folder_permission();

    // delete the sandbox folder if it existed
    if(fs::is_directory(CRETE_SANDBOX_PATH))
    {
        for (fs::directory_iterator end_dir_it, it((fs::path(CRETE_SANDBOX_PATH))); it!=end_dir_it; ++it)
        {
            int ret = umount(it->path().string().c_str());

            if(ret != 0)
            {
                fprintf(stderr, "umount() failed on: %s, check whether sys_cap_admin is set\n",
                        it->path().string().c_str());
            }
        }

        fs::remove_all(CRETE_SANDBOX_PATH);
        assert(!fs::exists(CRETE_SANDBOX_PATH) && "[crete-run] crete-sandbox folder reset failed!\n");
    }

    {
        const fs::path src = "/home";
        if(fs::is_directory(src))
        {
            const fs::path dst = fs::path(CRETE_SANDBOX_PATH) / "home";
            fs::create_directories(dst);
            rdonly_bind_mount(src, dst);
        }
    }
    {
        const fs::path src = "/lib";
        if(fs::is_directory(src))
        {
            const fs::path dst = fs::path(CRETE_SANDBOX_PATH) / "lib";
            fs::create_directories(dst);
            rdonly_bind_mount(src, dst);
        }
    }
    {
        const fs::path src = "/lib64";
        if(fs::is_directory(src))
        {
            const fs::path dst = fs::path(CRETE_SANDBOX_PATH) / "lib64";
            fs::create_directories(dst);
            rdonly_bind_mount(src, dst);
        }
    }
    {
        const fs::path src = "/usr";
        if(fs::is_directory(src))
        {
            const fs::path dst = fs::path(CRETE_SANDBOX_PATH) / "usr";
            fs::create_directories(dst);
            rdonly_bind_mount(src, dst);
        }
    }
    {
        const fs::path src = "/dev";
        if(fs::is_directory(src))
        {
            const fs::path dst = fs::path(CRETE_SANDBOX_PATH) / "dev";
            fs::create_directories(dst);
            rdonly_bind_mount(src, dst);
        }
    }
    {
        const fs::path src = "/proc";
        if(fs::is_directory(src))
        {
            const fs::path dst = fs::path(CRETE_SANDBOX_PATH) / "proc";
            fs::create_directories(dst);
            rdonly_bind_mount(src, dst);
        }
    }

    fs::create_directories(fs::path(CRETE_SANDBOX_PATH) / "tmp");
    fs::create_directories(fs::path(CRETE_SANDBOX_PATH) / CRETE_REPLAY_GCOV_PREFIX);
}

void CreteReplay::reset_sandbox()
{
    reset_sandbox_folder_permission();

    // 2. reset "sandbox-exec folder" within sandbox
    fs::path crete_sandbox_exec_path = fs::path(CRETE_SANDBOX_PATH) / m_launch_directory;
    fs::remove_all(crete_sandbox_exec_path);
    assert(fs::exists(fs::path(crete_sandbox_exec_path).parent_path()));

    bp::context ctx;
    ctx.stdout_behavior = bp::capture_stream();
    ctx.environment = bp::self::get_environment();

    std::string exec = bp::find_executable_in_path("cp");
    std::vector<std::string> args;
    args.push_back(exec);
    args.push_back("-r");
    args.push_back(m_input_sandbox.string());
    args.push_back(crete_sandbox_exec_path.string());

    bp::child c = bp::launch(exec, args, ctx);

    bp::pistream &is = c.get_stdout();

    // TODO: xxx should check the return status to make sure the "cp" completed successfully
    bp::status s = c.wait();
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
    if(m_input_sandbox.empty())
    {
        // when no sandbox, m_exec_launch_dir is set to the parent folder of the executable,
        // unless that folder is not writable (then it will be the working
        // directory of crete-run)
        m_launch_directory = m_exec.parent_path();
        if(access(m_launch_directory.string().c_str(), W_OK) != 0)
        {
            m_launch_directory = fs::current_path();
        }
    } else {
        m_launch_directory = fs::path("/tmp") / fs::canonical(m_input_sandbox).filename();
    }

    // 2. Set up m_launch_args
    config::Arguments guest_args = guest_config.get_arguments();

    m_launch_args.resize(guest_args.size()+1, string());
    m_launch_args[0] = m_exec.string();

    for(config::Arguments::const_iterator it = guest_args.begin();
            it != guest_args.end(); ++it) {
        assert(it->index < m_launch_args.size());
        assert(m_launch_args[it->index].empty());
        m_launch_args[it->index] = it->value;
    }

    // 3. Setup m_launch_ctx
    m_launch_ctx.output_behavior.insert(bp::behavior_map::value_type(STDOUT_FILENO, bp::capture_stream()));
    m_launch_ctx.output_behavior.insert(bp::behavior_map::value_type(STDERR_FILENO, bp::redirect_stream_to_stdout()));
    m_launch_ctx.input_behavior.insert(bp::behavior_map::value_type(STDIN_FILENO, bp::capture_stream()));

    //TODO: xxx unified the environment, may use klee's env.sh
    m_launch_ctx.environment = bp::self::get_environment();
    m_launch_ctx.environment.erase("LD_PRELOAD");
    m_launch_ctx.environment.insert(bp::environment::value_type("LD_PRELOAD", "libcrete_replay_preload.so"));

    m_launch_ctx.work_directory = m_launch_directory.string();

    if(!m_input_sandbox.empty())
    {
        m_launch_ctx.chroot = CRETE_SANDBOX_PATH;

        m_launch_ctx.environment.erase("GCOV_PREFIX");
        m_launch_ctx.environment.insert(bp::environment::value_type("GCOV_PREFIX", CRETE_REPLAY_GCOV_PREFIX));

        if(m_init_sandbox)
        {
            init_sandbox();
        }
    }

    // 4. setup the path for guest_config_serialized
    if(m_input_sandbox.empty())
    {
        m_guest_config_serialized = CRETE_CONFIG_SERIALIZED_PATH;
        m_current_tc = CRETE_REPLAY_CURRENT_TC;
    } else {
        m_guest_config_serialized = fs::path(CRETE_SANDBOX_PATH) / CRETE_CONFIG_SERIALIZED_PATH;
        m_current_tc = fs::path(CRETE_SANDBOX_PATH) / CRETE_REPLAY_CURRENT_TC;
    }
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

static bool end_with (std::string const &fullString, std::string const &ending)
{
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

void CreteReplay::collect_gcov_result()
{
//    fprintf(stderr, "collect_gcov_result() entered\n");

    // gcov data is in the right place if no sandbox is used
    if(m_input_sandbox.empty())
    {
        return;
    }

    // FIXME: xxx temp workwround that it make take a while for gcov to generate gcda files
    //        the sleep time of 1 seconds are subjective here
    sleep(1);

    fs::path gcov_data_dir = fs::path(CRETE_SANDBOX_PATH) / CRETE_REPLAY_GCOV_PREFIX;
    for ( boost::filesystem::recursive_directory_iterator end, it(gcov_data_dir);
            it!= end; ++it) {
        if(fs::is_directory(it->path()))
            continue;

        fs::path src = it->path();
        assert(fs::is_regular_file(src));
        if(!end_with(src.filename().string(), ".gcda"))
        {
            fprintf(stderr, "[crete-tc-replay] unexpected file: %s\n", src.string().c_str());
            assert(0);
        }

        assert(src.string().find(gcov_data_dir.string()) == 0);
        fs::path tgt(src.string().c_str() +  gcov_data_dir.string().length());
        assert(fs::is_directory(tgt.parent_path()));

//        fprintf(stderr, "copy from %s to %s\n",
//                src.string().c_str(),
//                tgt.string().c_str());

        fs::copy_file(src, tgt, fs::copy_option::overwrite_if_exists);
    }

//    fprintf(stderr, "collect_gcov_result() finished\n");
}

static unsigned monitored_pid = 0;
static unsigned monitored_timeout = 3;

static void timeout_handler(int signum)
{
    fprintf(stderr, "Send timeout (%d seconds) signal to its child process\n", monitored_timeout);
    assert(monitored_pid != 0);
    kill(monitored_pid, SIGUSR1);
}

static inline void init_timeout_handler()
{
    struct sigaction sigact;

    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = timeout_handler;
    sigaction(SIGALRM, &sigact, NULL);
}

static inline void process_exit_status(fs::ofstream& log, int exit_status)
{
    if(exit_status == 0)
    {
        log << "NORMAL EXIT STATUS.\n";
        return;
    }

    if((exit_status > CRETE_EXIT_CODE_SIG_BASE) &&
            (exit_status < (CRETE_EXIT_CODE_SIG_BASE + SIGUNUSED)) )
    {
        int signum = exit_status - CRETE_EXIT_CODE_SIG_BASE ;
        if(signum == SIGUSR1)
        {
            log << "Replay Timeout\n";
        } else {
            log << "[Signal Caught] signum = " << signum << ", signame: " << strsignal(signum) << endl;
        }
    }

    log << "ABNORMAL EXIT STATUS: " << exit_status << endl;
}

void CreteReplay::replay()
{
    init_timeout_handler();

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

        // prepare for replay
        {
            if(!m_input_sandbox.empty())
            {
                reset_sandbox();
            }


            // write replay_current_tc, for replay-preload to use
            fs::remove(m_current_tc);
            std::ofstream ofs(m_current_tc.string().c_str());
            if(!ofs.good())
            {
                BOOST_THROW_EXCEPTION(Exception() << err::file_open_failed(m_current_tc.string()));
            }
            it->write(ofs);
            ofs.close();

            // copy guest-config, for replay-preload to use
            try
            {
                fs::remove(m_guest_config_serialized);
                fs::copy(m_config, m_guest_config_serialized);
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
            bp::posix_child proc = bp::posix_launch(m_exec, m_launch_args, m_launch_ctx);

            monitored_pid = proc.get_id();
            assert(monitored_timeout != 0);
            alarm(monitored_timeout);

            ofs_replay_log << "Output from Launched executable:\n";
            bp::pistream& is = proc.get_stdout();
            std::string line;
            while(getline(is, line))
            {
                ofs_replay_log << line << endl;
            }
#endif

            bp::status status = proc.wait();
            alarm(0);
            process_exit_status(ofs_replay_log, status.exit_status());
        }

        ofs_replay_log << "====================================================================\n";
    }

    collect_gcov_result();
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
