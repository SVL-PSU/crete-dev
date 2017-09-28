#include <crete/common.h>
#include <crete/exception.h>
#include <crete/test_case.h>
#include <crete/harness_config.h>
#include <crete/tc-replay.h>

#include <boost/filesystem.hpp>
#include <boost/archive/binary_oarchive.hpp>

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

static const string replay_stdin_filename= "crete.replay.stdin.file";

namespace crete
{
class CreteReplayPreload
{
private:
    int m_argc;
    char **m_argv;

    config::HarnessConfiguration m_guest_config;
    map<string, TestCaseElement> m_current_tc;

    CheckExploitable m_ck_exp;

public:
    CreteReplayPreload(int argc, char **argv);
    ~CreteReplayPreload() {};

    void setup_concolic_args();
    void setup_concolic_files();
    void setup_concolic_stdin();

    void write_ck_exp();

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
    std::ifstream ifs(CRETE_CONFIG_SERIALIZED_PATH);

    if(!ifs.good())
    {
        BOOST_THROW_EXCEPTION(crete::Exception() << err::file_open_failed(CRETE_CONFIG_SERIALIZED_PATH));
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
    std::ifstream ifs(CRETE_REPLAY_CURRENT_TC);
    if(!ifs.good())
    {
        BOOST_THROW_EXCEPTION(Exception() <<
                err::file_open_failed(CRETE_REPLAY_CURRENT_TC));
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
            assert(it_file_libc!= m_current_tc.end());

            TestCaseElement file_libc= it_file_libc->second;
            m_current_tc.erase(it_file_libc);

            TestCaseElement selected_file = file_libc;

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
                if(it->path.filename().string() == fs::path(arg_it->value).filename().string()) {
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

            m_ck_exp.m_files.push_back(output_filename);
        } else {
            // sanity check
            assert(fs::exists(it->path) && fs::is_regular_file(it->path));
            assert(fs::file_size(it->path) == it->size);

            m_ck_exp.m_files.push_back(it->path.string());
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
            assert(it_stdin_libc!= m_current_tc.end());

            TestCaseElement stdin_libc= it_stdin_libc->second;
            m_current_tc.erase(it_stdin_libc);

            TestCaseElement selected_stdin= stdin_libc;

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

void CreteReplayPreload::write_ck_exp()
{
    m_ck_exp.m_p_launch = fs::current_path().string();
    m_ck_exp.m_p_exec = fs::canonical(m_argv[0]).string();
    for(uint64_t i = 0; i < m_argc; ++i)
    {
        m_ck_exp.m_args.push_back(string(m_argv[i]));
    }

    m_ck_exp.m_stdin_file = replay_stdin_filename;

    try {
        ofstream ofs(CRETE_TC_REPLAY_CK_EXP_INFO, ios_base::binary);
        boost::archive::binary_oarchive oa(ofs);
        oa << m_ck_exp;
        ofs.close();
    }
    catch(std::exception &e) {
        cerr << e.what() << endl;
        BOOST_THROW_EXCEPTION(crete::Exception()
        << err::msg("[crete-replay-preload] serialization error for writing ck_exp"));
    }
}

} //namespace crete

// ********************************************************* //
// Signal handling
#define __INIT_CRETE_SIGNAL_HANDLER(SIGNUM)                                  \
        {                                               \
            struct sigaction sigact;                    \
                                                        \
            memset(&sigact, 0, sizeof(sigact));         \
            sigact.sa_handler = crete_signal_handler;   \
            sigaction(SIGNUM, &sigact, NULL);           \
                                                        \
            sigset_t set;                               \
            sigemptyset(&set);                          \
            sigaddset(&set, SIGNUM);                    \
            sigprocmask(SIG_UNBLOCK, &set, NULL);       \
        }

// Terminate the program gracefully and return the corresponding exit code
// TODO: xxx does not handle nested signals
static void crete_signal_handler(int signum)
{
    //TODO: xxx _exit() is safer, but gcov will not generate coverage report with _exit()
    exit(CRETE_EXIT_CODE_SIG_BASE + signum);
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

    // SIGUSR1 for timeout
    __INIT_CRETE_SIGNAL_HANDLER(SIGUSR1);
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
    fprintf(stderr, "[replay-preload] prog: %s\n", ubp_av[0]);

    __libc_start_main_t orig_libc_start_main;

    try
    {
        unsetenv("LD_PRELOAD"); // Prevent nested LD_PRELOAD for processes started by the current process
        orig_libc_start_main = (__libc_start_main_t)dlsym(RTLD_NEXT, "__libc_start_main");

        if(orig_libc_start_main == 0)
        {
            BOOST_THROW_EXCEPTION(crete::Exception() << crete::err::msg("failed to find __libc_start_main"));
        }

        setpgrp();
        init_crete_signal_handlers(argc, ubp_av);
        crete::CreteReplayPreload crete_replay_preload(argc, ubp_av);
        crete_replay_preload.setup_concolic_args();
        crete_replay_preload.setup_concolic_files();
        crete_replay_preload.setup_concolic_stdin();
        crete_replay_preload.write_ck_exp();
    }
    catch(...)
    {
        std::cerr << "[crete-replay-preload] Exception: \n"
                << boost::current_exception_diagnostic_information() << std::endl;
        exit(-1);
    }

    for(int i = 0; i < argc; ++i)
    {
        string str_cmd(ubp_av[i]);
        fprintf(stderr, "argv[%d]: \'%s\' [", i, str_cmd.c_str());
        for(int j = 0; j < str_cmd.size(); ++j)
        {
            fprintf(stderr, "0x%x ", (unsigned int)str_cmd[j]);
        }
        fprintf(stderr, "] (%lu byte)\n", str_cmd.size());
    }

    (*orig_libc_start_main)(main, argc, ubp_av, init, fini, rtld_fini, stack_end);

    exit(1); // This is never reached. Doesn't matter, as atexit() is called upon returning from main().
}
