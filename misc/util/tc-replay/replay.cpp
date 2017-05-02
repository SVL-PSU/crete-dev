#include "replay.h"
#include <crete/common.h>
#include <crete/tc-replay.h>

#include <external/alphanum.hpp>

#include <boost/filesystem/fstream.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>

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
    m_init_sandbox(true),
    m_enable_log(false)
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
        ("environment,v", po::value<fs::path>(), "environment variables")
        ("log,l", po::bool_switch(), "enable log the output of replayed programs")
        ("exploitable-check,x", po::value<fs::path>(), "path to the output of exploitable-check")
        ("explo-check-script,r", po::value<fs::path>(), "path to the script to check exploitable with gdb replay")
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

    if(m_var_map.count("environment"))
    {
        fs::path p = m_var_map["environment"].as<fs::path>();

        if(!fs::exists(p) && !fs::is_regular(p))
        {
            BOOST_THROW_EXCEPTION(Exception() << err::file_missing(p.string()));
        }

        m_environment = p;
    }

    if(m_var_map.count("seed-only"))
    {
        m_seed_mode = true;
    } else {
        m_seed_mode = false;
    }

    if(m_var_map.count("log"))
    {
        bool input = m_var_map["log"].as<bool>();

        m_enable_log = input;
    }

    if(m_var_map.count("exploitable-check"))
    {
        m_exploitable_out = m_var_map["exploitable-check"].as<fs::path>();
        if(!fs::exists(m_exploitable_out))
        {
            fs::create_directories(m_exploitable_out);
        } else {
            CRETE_EXCEPTION_ASSERT(fs::is_directory(m_exploitable_out),
                    err::msg(m_exploitable_out.string() + "exists and is not a folder\n"));
        }

        if(!m_var_map.count("explo-check-script"))
        {
            BOOST_THROW_EXCEPTION(Exception() <<
                    err::file_missing("\'explo-check-script\' is required with \'exploitable-check\'"));
        }

        fs::path p= m_var_map["explo-check-script"].as<fs::path>();
        if(!fs::exists(p) && !fs::is_regular(p))
        {
            BOOST_THROW_EXCEPTION(Exception() << err::file_missing(p.string()));
        }
        m_exploitable_script = p;
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

    m_launch_ctx.work_directory = m_launch_directory.string();

    if(!m_environment.empty())
    {
        assert(m_launch_ctx.environment.empty());
        std::ifstream ifs (m_environment.string().c_str());
        if(!ifs.good())
            BOOST_THROW_EXCEPTION(Exception() << err::file_open_failed(m_environment.string()));

        std::string env_name;
        std::string env_value;
        while(ifs >> env_name >> env_value)
        {
            m_launch_ctx.environment.insert(bp::environment::value_type(env_name, env_value));
        }
    } else {
        m_launch_ctx.environment = bp::self::get_environment();
    }
    m_launch_ctx.environment.insert(bp::environment::value_type("LD_PRELOAD", "libcrete_replay_preload.so"));
    m_launch_ctx.environment.erase("PWD");
    m_launch_ctx.environment.insert(bp::environment::value_type("PWD", m_launch_ctx.work_directory));

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

    // exit() can cause deadlock within signal handlers, but it is required for coverage
    // Double kill the process
    sleep(1);
    kill(monitored_pid, SIGKILL);
}

static inline void init_timeout_handler()
{
    struct sigaction sigact;

    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = timeout_handler;
    sigaction(SIGALRM, &sigact, NULL);
}

// ret: true, if signal catched; false, if not
static inline bool process_exit_status(fs::ofstream& log, int exit_status)
{
    if(exit_status == 0)
    {
        log << "NORMAL EXIT STATUS.\n";
        return false;
    }

    bool ret = false;
    if((exit_status > CRETE_EXIT_CODE_SIG_BASE) &&
            (exit_status < (CRETE_EXIT_CODE_SIG_BASE + SIGUNUSED)) )
    {
        int signum = exit_status - CRETE_EXIT_CODE_SIG_BASE ;
        if(signum == SIGUSR1)
        {
            log << "Replay Timeout\n";
        } else {
            log << "[Signal Caught] signum = " << signum << ", signame: " << strsignal(signum) << endl;
            ret = true;
        }
    }

    log << "ABNORMAL EXIT STATUS: " << exit_status << endl;

    return ret;
}

static vector<string> get_files_ordered(const fs::path& input)
{
    CRETE_EXCEPTION_ASSERT(fs::exists(input),
            err::file_missing(input.string()));
    assert(fs::is_directory(input));

    // Sort the files alphabetically
    vector<string> file_list;
    for ( fs::directory_iterator itr( input );
          itr != fs::directory_iterator();
          ++itr ){
        file_list.push_back(itr->path().string());
    }

    sort(file_list.begin(), file_list.end(), doj::alphanum_less<string>());

    return file_list;
}

void CreteReplay::replay()
{
    init_timeout_handler();

    // Read all test cases to replay
    vector<string> test_list = get_files_ordered(m_tc_dir);

    fs::ofstream ofs_replay_log;

    if(m_enable_log)
    {
        ofs_replay_log.open(m_cwd / replay_log_file, std::ios_base::app);
    } else {
        ofs_replay_log.open("/dev/null");
    }

    ofs_replay_log << "Replay Summary: [" << currentDateTime() << "]\n"
            << "Executable path: " << m_exec.string() << endl
            << "Test case directory path: " << m_tc_dir.string() << endl
            << "Guest config path: " << m_config.string() << endl
            << "Working directory: " << m_cwd.string() << endl
            << "Launch direcotory: " << m_launch_directory.string() << endl
            << "Number of test cases: " << test_list.size() << endl
            << endl;

    uint64_t replayed_tc_count = 1;
    for (vector<string>::const_iterator it(test_list.begin()), it_end(test_list.end());
            it != it_end; ++it)
    {
        if(m_seed_mode && (replayed_tc_count != 1))
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
            fs::copy(*it, m_current_tc);

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
            stringstream ss_prog_out;
            while(getline(is, line))
            {
                ofs_replay_log << line << endl;
                ss_prog_out << line << endl;
            }
#endif

            bp::status status = proc.wait();
            alarm(0);

            bool signal_caught;
            if(status.exited())
            {
                signal_caught = process_exit_status(ofs_replay_log, status.exit_status());
            } else {
                // When the child process is not terminated from exit()/_exit(),
                // assuming there is a signal caught.
                signal_caught = true;
            }

            if(signal_caught)
            {
                check_exploitable(*it, ss_prog_out.str());
            }
        }

        ofs_replay_log << "====================================================================\n";
    }

    collect_gcov_result();
}

// FIXME: xxx add timeout to deal with GDB hanging
static vector<string> run_gdb_script(const CheckExploitable& ck_exp,
        const string& script)
{
    cerr << "run_gdb_script() entered\n";

    bp::context ctx;
    ctx.stdout_behavior = bp::capture_stream();
    ctx.stderr_behavior = bp::redirect_stream_to_stdout();
    ctx.stdin_behavior = bp::capture_stream();
    ctx.work_directory = ck_exp.m_p_launch;

    fs::copy_file(CRETE_TC_REPLAY_GDB_SCRIPT,
            fs::path(ctx.work_directory) / CRETE_TC_REPLAY_GDB_SCRIPT,
            fs::copy_option::overwrite_if_exists);

    std::string exec = bp::find_executable_in_path("gdb");
    std::vector<std::string> args;
    args.push_back("gdb");
    args.push_back("-x");
    args.push_back(script);

    bp::child c = bp::launch(exec, args, ctx);

    monitored_pid = c.get_id();
    assert(monitored_timeout != 0);
    alarm(monitored_timeout*3);

    bp::pistream &is = c.get_stdout();
    std::string line;

    vector<string> gdb_out;
    while (std::getline(is, line))
    {
        gdb_out.push_back(line);
    }

    alarm(0);

    cerr << "run_gdb_script() finished\n";
    return gdb_out;
}

static fs::path prepare_explo_dir(const CheckExploitable& ck_exp,
        const CheckExploitableResult& result, const fs::path out_dir)
{
    CRETE_EXCEPTION_ASSERT(fs::is_directory(out_dir),
            err::file_missing(out_dir.string()));

    fs::path parsed_explo = out_dir;
    if(!fs::exists(parsed_explo))
    {
        fs::create_directories(parsed_explo);
    } else {
        CRETE_EXCEPTION_ASSERT(fs::is_directory(parsed_explo),
                err::msg(parsed_explo.string() + "exists and is not a folder\n"));
    }

    fs::path prog_out = parsed_explo / fs::path(ck_exp.m_p_exec).filename();
    if(!fs::exists(prog_out))
    {
        fs::create_directories(prog_out);
    } else {
        CRETE_EXCEPTION_ASSERT(fs::is_directory(parsed_explo),
                err::msg(prog_out.string() + "exists and is not a folder\n"));
    }

    fs::path explo_out = prog_out / (result.m_exp_ty_msg + "-" + result.m_hash);
    if(fs::exists(explo_out))
    {
        CRETE_EXCEPTION_ASSERT(fs::is_directory(parsed_explo),
                err::msg(explo_out.string() + "exists and is not a folder\n"));

        explo_out = explo_out / "others";
        if(!fs::exists(explo_out))
        {
            fs::create_directories(explo_out);
        } else {
            CRETE_EXCEPTION_ASSERT(fs::is_directory(parsed_explo),
                    err::msg(explo_out.string() + "exists and is not a folder\n"));
        }

        for (int i = 1; ; i++) {
            fs::path dirPath = explo_out / boost::lexical_cast<std::string>(i);
            if(!fs::exists(dirPath)) {
                explo_out = dirPath.string();
                break;
            }
        }
    }

    assert(!fs::exists(explo_out));
    fs::create_directories(explo_out);

    return explo_out;
}

static void write_exploitable_log(const CheckExploitable& ck_exp,
        const vector<string>& gdb_out, const fs::path out_dir,
        const fs::path& tc_path, const string& replay_log)
{
    CheckExploitableResult result(gdb_out);

    fs::path explo_out = prepare_explo_dir(ck_exp, result, out_dir);
    assert(fs::is_directory(explo_out));

    fs::path exe_launch_dir = ck_exp.m_p_launch;
    // 1. gdb_script
    fs::copy_file(exe_launch_dir / CRETE_TC_REPLAY_GDB_SCRIPT,
            explo_out / CRETE_TC_REPLAY_GDB_SCRIPT);

    // 2. all files
    for(uint64_t i = 0; i < ck_exp.m_files.size(); ++i)
    {
        fs::copy_file(exe_launch_dir / ck_exp.m_files[i],
                    explo_out / ck_exp.m_files[i]);
    }

    fs::copy_file(exe_launch_dir / ck_exp.m_stdin_file,
                explo_out / ck_exp.m_stdin_file);

    // 3. summary_log
    ofstream ofs((explo_out / CRETE_EXPLO_SUMMARY_LOG).string().c_str());

    ofs << "================\n"
        << "Exploitable log:\n"
        << "================\n\n";

    ofs << "Exploitability Classification: " << result.m_exp_ty_msg << endl
        << "Description: " << result.m_description << endl
        << "Explanation: " << result.m_explanation << endl << endl
        << "Note: generated by \"GDB 'exploitable' plugin\" (https://github.com/jfoote/exploitable)"
        << endl << endl;

    ofs << "=================\n"
        << "Complete GDB log:\n"
        << "=================\n\n";

    for(uint64_t i = 0; i < gdb_out.size(); ++i)
    {
        ofs << gdb_out[i] << endl;
    }

    ofs << "==========\n"
        << "CRETE log:\n"
        << "==========\n\n";

    ofs << "crete-tc: " << tc_path.string() << endl << endl;

    ofs << "-----------------------\n"
        << "crete-tc-replay output:\n"
        << "-----------------------\n\n"
        << replay_log << endl;


    ofs.close();
}

void CreteReplay::check_exploitable(const fs::path& tc_path,
        const string& replay_log) const
{
    if(m_exploitable_script.empty())
        return;

    assert(m_input_sandbox.empty() &&
            "[CRETE ERROR] NOT support check for exploitable with sandbox replay.\n");

    cerr << "check_exploitable: " << tc_path.string() << endl;
    assert(fs::exists(CRETE_TC_REPLAY_CK_EXP_INFO));
    ifstream ifs(CRETE_TC_REPLAY_CK_EXP_INFO, ios_base::binary);
    boost::archive::binary_iarchive ia(ifs);

    CheckExploitable ck_exp;
    ia >> ck_exp;

    assert(ck_exp.m_p_launch == m_launch_directory.string());
    ck_exp.m_p_exploitable_script = m_exploitable_script.string();
    ck_exp.gen_gdb_script(CRETE_TC_REPLAY_GDB_SCRIPT);

    vector<string> gdb_out = run_gdb_script(ck_exp, CRETE_TC_REPLAY_GDB_SCRIPT);

    write_exploitable_log(ck_exp, gdb_out, m_exploitable_out,
            tc_path, replay_log);
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
