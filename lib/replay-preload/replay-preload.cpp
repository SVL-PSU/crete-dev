#include <crete/exception.h>
#include <crete/test_case.h>
#include <crete/harness_config.h>

#include <boost/filesystem.hpp>

#include <signal.h>
#include <dlfcn.h>
#include <iostream>
#include <fstream>

extern "C" {
    // The type of __libc_start_main
    typedef int (*__libc_start_main_t)(
            int *(main) (int, char**, char**),
            int argc,
            char ** ubp_av,
            void (*init) (void),
            void (*fini) (void),
            void (*rtld_fini) (void),
            void (*stack_end)
            );

    int __libc_start_main(
            int *(main) (int, char **, char **),
            int argc,
            char ** ubp_av,
            void (*init) (void),
            void (*fini) (void),
            void (*rtld_fini) (void),
            void *stack_end)
            __attribute__ ((noreturn));
}

using namespace std;
namespace fs = boost::filesystem;

static const string replay_log_file = "crete.replay.log";
static const string replay_current_tc = "crete.replay.current.tc.bin";
static const string replay_stdin_filename= "crete.replay.stdin.file";
static const string replay_guest_config = "crete.replay.guest.config.serialized";

namespace crete
{
class CreteReplayPreload
{
private:
    int m_argc;
    char **m_argv;

    config::HarnessConfiguration m_guest_config;
    map<string, TestCaseElement> m_current_tc;

public:
    CreteReplayPreload(int argc, char **argv);
    ~CreteReplayPreload() {};

    void setup_concolic_args();
    void setup_concolic_files();
    void setup_concolic_stdin();

private:
    void init_guest_config();
    void init_current_tc();
};


CreteReplayPreload::CreteReplayPreload(int argc, char **argv):
        m_argc(argc), m_argv(argv)
{
    init_guest_config();
    init_current_tc();
}

void CreteReplayPreload::init_guest_config()
{
    std::ifstream ifs(replay_guest_config.c_str());

    if(!ifs.good())
    {
        BOOST_THROW_EXCEPTION(crete::Exception() << err::file_open_failed(replay_guest_config));
    }

    try
    {
        boost::archive::text_iarchive ia(ifs);
        ia >> m_guest_config;
    }
    catch(std::exception& e)
    {
        cerr << boost::diagnostic_information(e) << endl;
        BOOST_THROW_EXCEPTION(e);
    };

    ifs.close();
}
void CreteReplayPreload::init_current_tc()
{
    std::ifstream ifs(replay_current_tc.c_str());
    if(!ifs.good())
    {
        BOOST_THROW_EXCEPTION(Exception() <<
                err::file_open_failed(replay_current_tc));
    }

    TestCase tc = read_test_case(ifs);
    for(vector<TestCaseElement>::const_iterator tc_iter = tc.get_elements().begin();
            tc_iter !=  tc.get_elements().end();
            ++tc_iter)
    {
        uint32_t size = tc_iter->data_size;
        string name(tc_iter->name.begin(), tc_iter->name.end());
        vector<uint8_t> data = tc_iter->data;

        assert(size == data.size());
        assert(name.length() == tc_iter->name_size);

        m_current_tc.insert(make_pair(name, *tc_iter));
    }
}

// Replace the value of conoclic argument with the values from test case
void CreteReplayPreload::setup_concolic_args()
{
    cerr << "setup_concolic_args()\n";

    const config::Arguments& guest_config_args = m_guest_config.get_arguments();

    // Replace the value of conoclic argument with the values from test case
    for(config::Arguments::const_iterator it = guest_config_args.begin();
            it != guest_config_args.end(); ++it) {
        if(it->concolic)
        {
            assert(it->index < m_argc);

            stringstream concolic_name;
            concolic_name << "argv_" << it->index;

            map<string, TestCaseElement>::iterator it_current_tc_arg =
                    m_current_tc.find(concolic_name.str());
            assert(it_current_tc_arg != m_current_tc.end());

            TestCaseElement current_tc_arg = it_current_tc_arg->second;
            m_current_tc.erase(it_current_tc_arg);
            assert(current_tc_arg.data.size() == current_tc_arg.data_size);

            char *buffer = new char [current_tc_arg.data_size + 1];
            memcpy(buffer, current_tc_arg.data.data(), current_tc_arg.data_size);
            buffer[current_tc_arg.data_size] = '\0';
            m_argv[it->index] = buffer;
        } else {
            // sanity check
            // Note: string::compare() will not work, as it->value
            //       can contain several '\0' at the end while argv doesn't
            assert(strcmp(it->value.c_str(), m_argv[it->index]) == 0 &&
                    "[run-preload] Sanity check on concrete args failed\n");
        }
    }
}

TestCaseElement select_file(const TestCaseElement& lhs,
        const TestCaseElement& rhs)
{
    assert(lhs.data.size() == rhs.data.size());

    std::vector<uint8_t> zero_v(lhs.data.size(), 0);

    bool lhs_z = (lhs.data == zero_v);
    bool rhs_z = (rhs.data == zero_v);

    assert((lhs_z || rhs_z) &&
            "[crete-replay-preload Error] Both symfile-libc and symfile-posix are not empty.");

    // Both of them are blank
    if(lhs_z && rhs_z)
    {
        fprintf(stderr, "[select_file] Both %s and %s are empty\n",
                string(lhs.name.begin(), lhs.name.end()).c_str(),
                string(rhs.name.begin(), rhs.name.end()).c_str());

        return lhs;
    }

    if(!lhs_z){
        fprintf(stderr, "[select_file] Only %s is not empty\n",
                string(lhs.name.begin(), lhs.name.end()).c_str());

        return lhs;
    } else {
        assert(!rhs_z);

        fprintf(stderr, "[select_file] Only %s is not empty\n",
                string(rhs.name.begin(), rhs.name.end()).c_str());

        return rhs;
    }
}

// Generate a concrete file with the content of conoclic file in test case
// Also, adjust the command-line input to make sure the file name specified in
// argv is consistent with the file we generated here
void CreteReplayPreload::setup_concolic_files()
{
    config::Arguments guest_config_args = m_guest_config.get_arguments();
    const config::Files& guest_config_files = m_guest_config.get_files();

    for(config::Files::const_iterator it = guest_config_files.begin();
            it != guest_config_files.end(); ++it) {
        if(it->concolic)
        {
            // 1. Get concolic file data
            string filename = it->path.filename().string();

            map<string, TestCaseElement>::iterator it_file_libc=
                    m_current_tc.find(filename);
            map<string, TestCaseElement>::iterator it_file_posix=
                    m_current_tc.find(filename + "-posix");
            assert(it_file_libc!= m_current_tc.end());
            assert(it_file_posix != m_current_tc.end());

            TestCaseElement file_libc= it_file_libc->second;
            TestCaseElement file_posix = it_file_posix->second;
            m_current_tc.erase(it_file_libc);
            m_current_tc.erase(it_file_posix);

            TestCaseElement selected_file = select_file(file_libc, file_posix);

            // 2. Write out concolic
            string output_filename = "crete.replay." + string(selected_file.name.begin(), selected_file.name.end());
            std::ofstream ofs(output_filename.c_str());
            if(!ofs.good())
            {
                BOOST_THROW_EXCEPTION(crete::Exception() << err::file_create(filename));
            }

            assert(selected_file.data.size() == selected_file.data_size);
            ofs.write((char *)selected_file.data.data(), selected_file.data.size());

            // 2. Adjust argv to the output_filename
            bool found = false;
            for(config::Arguments::iterator arg_it = guest_config_args.begin();
                    arg_it != guest_config_args.end(); ++arg_it) {
                if(it->path.string().find(arg_it->value) != string::npos) {
                    assert(it->path.filename().string() == fs::path(arg_it->value).filename().string());
                    assert(arg_it->index < m_argc);
                    assert(!arg_it->concolic && "Symbolic file content with symbolic file path\n");

                    found = true;
                    char *buffer = new char [output_filename.size()+ 1];
                    memcpy(buffer, output_filename.data(), output_filename.size());
                    buffer[output_filename.size()] = '\0';
                    m_argv[arg_it->index] = buffer;

                    guest_config_args.erase(arg_it);
                    break;
                }
            }

            assert(found);
        } else {
            // sanity check
            assert(fs::exists(it->path) && fs::is_regular_file(it->path));
            assert(fs::file_size(it->path) == it->size);
        }
    }
}

void CreteReplayPreload::setup_concolic_stdin()
{
    config::STDStream guest_config_stdin = m_guest_config.get_stdin();
    if(guest_config_stdin.size > 0) {
        std::ofstream ofs(replay_stdin_filename.c_str());
        if(!ofs.good())
        {
            BOOST_THROW_EXCEPTION(crete::Exception() << err::file_create(replay_stdin_filename));
        }
        assert(guest_config_stdin.value.size() == guest_config_stdin.size);

        if(guest_config_stdin.concolic)
        {
            // 1. Get concolic file data
            map<string, TestCaseElement>::iterator it_stdin_libc=
                    m_current_tc.find("crete-stdin");
            map<string, TestCaseElement>::iterator it_stdin_posix=
                    m_current_tc.find("crete-stdin-posix");
            assert(it_stdin_libc!= m_current_tc.end());
            assert(it_stdin_posix != m_current_tc.end());

            TestCaseElement stdin_libc= it_stdin_libc->second;
            TestCaseElement stdin_posix = it_stdin_posix->second;
            m_current_tc.erase(it_stdin_libc);
            m_current_tc.erase(it_stdin_posix);

            TestCaseElement selected_stdin= select_file(stdin_libc, stdin_posix);

            // 2. Write out concolic stdin
            assert(selected_stdin.data.size() == selected_stdin.data_size);
            ofs.write((char *)selected_stdin.data.data(),selected_stdin.data.size());
        } else {
            ofs.write((char *)guest_config_stdin.value.data(), guest_config_stdin.size);
        }

        ofs.close();

        //3. Redirect stdin to file "crete_stdin_ramdisk"
        //TODO: xxx shall I open it with flag "b" (binary)?
        if(freopen(replay_stdin_filename.c_str(), "rb", stdin) == NULL) {
            fprintf(stderr, "Error: Redirect stdin to %s by freopen() failed.\n",
                    replay_stdin_filename.c_str());
        }
    }
}

} //namespace crete

// ********************************************************* //
// Signal handling
static sighandler_t default_signal_hanlders[_NSIG];

#define __INIT_CRETE_SIGNAL_HANDLER(SIGNUM)                                     \
        default_signal_hanlders[SIGNUM] = signal(SIGNUM, crete_signal_handler); \
        assert(default_signal_hanlders[SIGNUM] != SIG_ERR);

// Log the signal and call the default signal handler
// TODO: xxx A certain kind of signal will only be caught once
static void crete_signal_handler(int signum)
{
    static int caught_signal_count = 0;

    fprintf(stderr, "[Signal Caught](%d): signum = %d (%s)\n",
            caught_signal_count, signum, strsignal(signum));

    signal(signum, default_signal_hanlders[signum]);
    raise(signum);
}

static void init_crete_signal_handlers(int argc, char **argv)
{
    __INIT_CRETE_SIGNAL_HANDLER(SIGABRT);
    __INIT_CRETE_SIGNAL_HANDLER(SIGFPE);
    __INIT_CRETE_SIGNAL_HANDLER(SIGHUP);
    __INIT_CRETE_SIGNAL_HANDLER(SIGINT);
    __INIT_CRETE_SIGNAL_HANDLER(SIGILL);
    __INIT_CRETE_SIGNAL_HANDLER(SIGPIPE);
    __INIT_CRETE_SIGNAL_HANDLER(SIGQUIT);
    __INIT_CRETE_SIGNAL_HANDLER(SIGSEGV);
    __INIT_CRETE_SIGNAL_HANDLER(SIGSYS);
    __INIT_CRETE_SIGNAL_HANDLER(SIGTERM);
    __INIT_CRETE_SIGNAL_HANDLER(SIGTSTP);
    __INIT_CRETE_SIGNAL_HANDLER(SIGXCPU);
    __INIT_CRETE_SIGNAL_HANDLER(SIGXFSZ);
}

// ********************************************************* //
// Hook routines for supporting crete intrinics

// TODO: xxx handle crete_make_concolic for replaying
void crete_make_concolic(uint64_t addr, uint64_t size, char *name)
{
    ;
}

int __libc_start_main(
        int *(main) (int, char **, char **),
        int argc,
        char ** ubp_av,
        void (*init) (void),
        void (*fini) (void),
        void (*rtld_fini) (void),
        void *stack_end) {
    fprintf(stderr, "[replay-prealod] prog: %s\n", ubp_av[0]);

    __libc_start_main_t orig_libc_start_main;

    try
    {
        unsetenv("LD_PRELOAD"); // Prevent nested LD_PRELOAD for processes started by the current process
        orig_libc_start_main = (__libc_start_main_t)dlsym(RTLD_NEXT, "__libc_start_main");

        if(orig_libc_start_main == 0)
        {
            BOOST_THROW_EXCEPTION(crete::Exception() << crete::err::msg("failed to find __libc_start_main"));
        }

        init_crete_signal_handlers(argc, ubp_av);
        crete::CreteReplayPreload crete_replay_preload(argc, ubp_av);
        crete_replay_preload.setup_concolic_args();
        crete_replay_preload.setup_concolic_files();
        crete_replay_preload.setup_concolic_stdin();
    }
    catch(...)
    {
        std::cerr << "[crete-replay-preload] Exception: \n"
                << boost::current_exception_diagnostic_information() << std::endl;
        exit(-1);
    }

    for(int i = 0; i < argc; ++i)
    {
        cout << "argv[" << i << "]: " << ubp_av[i] << endl;
    }

    (*orig_libc_start_main)(main, argc, ubp_av, init, fini, rtld_fini, stack_end);

    exit(1); // This is never reached. Doesn't matter, as atexit() is called upon returning from main().
}
