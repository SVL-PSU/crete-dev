#include "runner.h"

#include <crete/common.h>
#include <crete/run_config.h>
#include <crete/custom_instr.h>
#include <crete/exception.h>
#include <crete/process.h>
#include <crete/asio/client.h>

#include <boost/process.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/range/irange.hpp>
#include <boost/msm/back/state_machine.hpp> // back-end
#include <boost/msm/front/state_machine_def.hpp> //front-end
#include <boost/msm/front/functor_row.hpp>
#include <boost/thread.hpp>
#include <boost/property_tree/detail/file_parser_error.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>

#include <unistd.h>
#include <sys/mount.h>

namespace bp = boost::process;
namespace fs = boost::filesystem;
namespace msm = boost::msm;
namespace mpl = boost::mpl;
namespace pt = boost::property_tree;
namespace ba = boost::algorithm;
namespace po = boost::program_options;
using namespace msm::front;

namespace crete
{
static const std::string log_file_name = "run.log";
// +--------------------------------------------------+
// + Finite State Machine                             +
// +--------------------------------------------------+

// +--------------------------------------------------+
// + Flags                                            +
// +--------------------------------------------------+
namespace flag
{
//    struct error {};
    struct next_test {};
}

// +--------------------------------------------------+
// + Events                                           +
// +--------------------------------------------------+
struct start;
struct poll {};
struct next_test{};

// +--------------------------------------------------+
// + State Machine Front End                          +
// +--------------------------------------------------+
class RunnerFSM_ : public boost::msm::front::state_machine_def<RunnerFSM_>
{
private:
    std::string host_ip_;
    std::string host_port_;
    boost::shared_ptr<Client> client_;
    fs::path guest_config_path_;
    config::RunConfiguration guest_config_;

    fs::path m_exec_launch_dir;
    fs::path m_exec;
    std::vector<std::string> m_launch_args;
    bp::posix_context m_launch_ctx;

    fs::path m_sandbox_dir;
    fs::path m_proc_map;
    fs::path m_guest_config_serialized;

    pid_t pid_;

    bool is_first_exec_;
    std::size_t proc_maps_hash_;

public:
    RunnerFSM_();

    void prime_virtual_machine();
    void prime_executable();
    void write_configuration() const;
    void launch_executable();
    void signal_dump() const;

    void process_func_filter(ProcReader& pr,
                             const config::Functions& funcs,
                             void (*f_custom_instr)(uintptr_t, uintptr_t));
    void process_lib_filter(ProcReader& pr,
                            const std::vector<std::string>& libs,
                            void (*f_custom_instr)(uintptr_t, uintptr_t));
    void process_executable_section(ELFReader& reader,
                                    const std::vector<std::string>& sections,
                                    void (*f_custom_instr)(uintptr_t, uintptr_t));
    void process_call_stack_library_exclusions(ELFReader& er,
                                               const ProcReader& pr);
    void process_call_stack_library_exclusions(const ProcReader& pr,
                                               const std::vector<boost::filesystem::path>& libraries);
    void process_library_sections(const ProcReader& pr);
    void process_library_section(ELFReader& reader,
                                 const std::vector<std::string>& sections,
                                 void (*f_custom_instr)(uintptr_t, uintptr_t),
                                 uint64_t base_addr);
    void process_executable_function_entries(const std::vector<Entry>& entries);
    void process_library_function_entries(const std::vector<Entry>& entries,
                                          uint64_t base_addr,
                                          std::string path);
    fs::path deduce_library(const fs::path& lib,
                            const ProcReader& pr);

private:
    void setup_launch_exec();

    void reset_sandbox();

    void init_launch_folder();

    void init_sandbox();
    void init_ramdisk();
    void reset_sandbox_folder_permission();

public:
    // +--------------------------------------------------+
    // FSM                                                +
    // +--------------------------------------------------+

    // +--------------------------------------------------+
    // + Entry & Exit                                     +
    // +--------------------------------------------------+
    template <class Event,class FSM>
    void on_entry(Event const&,FSM&)
    {
        std::cerr << "[crete-run] entering: RunnerFSM_" << std::endl;
    }
    template <class Event,class FSM>
    void on_exit(Event const&,FSM&)
    {
        std::cerr << "[crete-run] leaving: RunnerFSM_" << std::endl;
    }

    // +--------------------------------------------------+
    // + States                                           +
    // +--------------------------------------------------+
    struct Start : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cerr << "[crete-run] entering: Start" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cerr << "[crete-run] leaving: Start" << std::endl;}
    };
    struct VerifyEnv : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cerr << "[crete-run] entering: VerifyEnv" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cerr << "[crete-run] leaving: VerifyEnv" << std::endl;}
    };
    struct Clean : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cerr << "[crete-run] entering: Clean" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cerr << "[crete-run] leaving: Clean" << std::endl;}
    };
    struct ConnectHost : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cerr << "[crete-run] entering: ConnectHost" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cerr << "[crete-run] leaving: ConnectHost" << std::endl;}
    };
    struct LoadHostData : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cerr << "[crete-run] entering: LoadHostData" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cerr << "[crete-run] leaving: LoadHostData" << std::endl;}
    };
    struct LoadDefaults : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cerr << "[crete-run] entering: LoadDefaults" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cerr << "[crete-run] leaving: LoadDefaults" << std::endl;}
    };
    struct LoadInputData : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cerr << "[crete-run] entering: LoadInputData" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cerr << "[crete-run] leaving: LoadInputData" << std::endl;}
    };
    struct TransmitGuestData : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cerr << "[crete-run] entering: TransmitGuestData" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cerr << "[crete-run] leaving: TransmitGuestData" << std::endl;}
    };
    struct Prime : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cerr << "[crete-run] entering: Prime" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cerr << "[crete-run] leaving: Prime" << std::endl;}
    };
    struct ProcessConfig : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cerr << "[crete-run] entering: ProcessConfig" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cerr << "[crete-run] leaving: ProcessConfig" << std::endl;}
    };
    struct ValidateSetup : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cerr << "[crete-run] entering: ValidateSetup" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cerr << "[crete-run] leaving: ValidateSetup" << std::endl;}
    };
    struct UpdateConfig : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cerr << "[crete-run] entering: UpdateConfig" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cerr << "[crete-run] leaving: UpdateConfig" << std::endl;}
    };
    struct Execute : public msm::front::state<>
    {
        typedef mpl::vector1<flag::next_test> flag_list;

        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cerr << "[crete-run] entering: Execute" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cerr << "[crete-run] leaving: Execute" << std::endl;}
    };
    struct Finished : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cerr << "[crete-run] entering: Finished" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cerr << "[crete-run] leaving: Finished" << std::endl;}
    };
    struct VerifyInvariants : public msm::front::state<>
    {
        template <class Event,class FSM>
        void on_entry(Event const& ,FSM&) {std::cerr << "[crete-run] entering: VerifyInvariants" << std::endl;}
        template <class Event,class FSM>
        void on_exit(Event const&,FSM& ) {std::cerr << "[crete-run] leaving: VerifyInvariants" << std::endl;}
    };

    // +--------------------------------------------------+
    // + Actions                                          +
    // +--------------------------------------------------+
    void init(const start&);
    void verify_env(const poll&);
    void clean(const poll&);
    void connect_host(const poll&);
    void load_host_data(const poll&);
    void load_defaults(const poll&);
    void load_file_data(const poll&);
    void transmit_guest_data(const poll&);
    void prime(const poll&);
    void process_config(const poll&);
    void validate_setup(const poll&);
    void update_config(const poll&);
    void execute(const next_test&);
    void finished(const poll&);
    void verify_invariants(const poll&);

    // +--------------------------------------------------+
    // + Gaurds                                           +
    // +--------------------------------------------------+

    bool is_process_finished(const poll&);
    bool is_first_exec(const poll&);
    bool is_not_first_exec(const poll&);

    // +--------------------------------------------------+
    // + Transitions                                      +
    // +--------------------------------------------------+

    template <class FSM,class Event>
    void exception_caught (Event const&,FSM& fsm,std::exception& e)
    {
        // TODO: transition to error state.
//        fsm.process_event(ErrorConnection());
        std::cerr << boost::diagnostic_information(e) << std::endl;
        assert(0 && "RunnerFSM_: exception thrown from within FSM");
    }

    typedef RunnerFSM_ M;

    // Initial state of the FSM.
    typedef Start initial_state;

    struct transition_table : mpl::vector
    <
    //    Start              Event              Target             Action                Guard
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<Start             ,start             ,VerifyEnv         ,&M::init                                  >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<VerifyEnv         ,poll              ,Clean             ,&M::verify_env                            >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<Clean             ,poll              ,ConnectHost       ,&M::clean                                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<ConnectHost       ,poll              ,LoadHostData      ,&M::connect_host                          >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<LoadHostData      ,poll              ,LoadDefaults      ,&M::load_host_data                        >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<LoadDefaults      ,poll              ,LoadInputData     ,&M::load_defaults                         >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<LoadInputData     ,poll              ,TransmitGuestData ,&M::load_file_data                       >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<TransmitGuestData ,poll              ,Prime             ,&M::transmit_guest_data                   >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<Prime             ,poll              ,ProcessConfig     ,&M::prime                                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<ProcessConfig     ,poll              ,ValidateSetup     ,&M::process_config                        >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<ValidateSetup     ,poll              ,Execute           ,&M::validate_setup                        >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<Execute           ,next_test         ,Finished          ,&M::execute                               >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      row<Finished          ,poll              ,VerifyInvariants  ,&M::finished         ,&M::is_process_finished >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      row<VerifyInvariants  ,poll              ,UpdateConfig      ,&M::verify_invariants,&M::is_first_exec   >,
      row<VerifyInvariants  ,poll              ,Execute           ,&M::verify_invariants,&M::is_not_first_exec   >,
    //   +------------------+------------------+------------------+---------------------+------------------+
    a_row<UpdateConfig      ,poll              ,Execute           ,&M::update_config                          >
    > {};
};

// Normally would just: "using RunnerFSM = boost::msm::back::state_machine<RunnerFSM>;",
// but needed a way to hide the impl. in source file, so as to avoid namespace troubles.
// This seems to work.
class RunnerFSM : public boost::msm::back::state_machine<RunnerFSM_>
{
};

struct start // Basically, serves as constructor.
{
    start(const std::string& host_ip,
          const fs::path& config,
          const fs::path& sandbox) :
        host_ip_(host_ip),
        config_(config),
        sandbox_(sandbox)
    {}

    const std::string& host_ip_;
    const fs::path& config_;
    const fs::path& sandbox_;
};

RunnerFSM_::RunnerFSM_() :
    client_(),
    pid_(-1),
    is_first_exec_(true),
    proc_maps_hash_(0)
{
}

void RunnerFSM_::init(const start& ev)
{
    host_ip_ = ev.host_ip_;
    guest_config_path_ = ev.config_;

    m_sandbox_dir = ev.sandbox_;
}

void RunnerFSM_::verify_env(const poll&)
{
    // TODO: xxx check ASLR disabled
}

void RunnerFSM_::clean(const poll&)
{
    fs::remove(log_file_name);
}

// NOTE: blocking function
void RunnerFSM_::connect_host(const poll&)
{
    std::cerr << "[CRETE] Waiting for port..." << std::endl;

    Port port = 0;

    //TODO: xxx improve the check on port validity
    do
    {
        crete_insert_instr_read_port((uintptr_t)&port, (uintptr_t)sizeof(port));

    }while(port == 0);

    std::cerr << "[CRETE] Connecting to host '"
              << host_ip_
              << "' on port "
              << port
              << "' ..."
              << std::endl;

    client_ = boost::make_shared<Client>(host_ip_,
                                         boost::lexical_cast<std::string>(port));

    client_->connect();
}

void RunnerFSM_::load_host_data(const poll&)
{
    bool distributed = false;

    read_serialized_text(*client_,
                         distributed);

    if(distributed)
    {
        if(!guest_config_path_.empty())
        {
            BOOST_THROW_EXCEPTION(Exception() << err::mode("target configuration file provided while in 'distributed' mode")
                                              << err::msg("please use omit the argument, or use 'developer' mode"));
        }

        std::string tmp;

        read_serialized_text(*client_,
                             tmp);

        guest_config_path_ = tmp;
    }
    else if(guest_config_path_.empty())
    {
        BOOST_THROW_EXCEPTION(Exception() << err::mode("target configuration file NOT provided while in 'developer' mode")
                                          << err::msg("please provide the argument, or use 'distributed' mode"));
    }
}

void RunnerFSM_::load_defaults(const poll&)
{
    try
    {
        guest_config_ = config::RunConfiguration(guest_config_path_);
    }
    catch(pt::file_parser_error& e)
    {
        BOOST_THROW_EXCEPTION(Exception() << err::parse(guest_config_path_.string())
                                          << err::msg(e.what()));
    }
}

static unsigned monitored_pid = 0;
static unsigned monitored_timeout = 3;

static void timeout_handler(int signum)
{
    if(monitored_pid)
    {
        kill(monitored_pid, SIGUSR1);
    } else {
        _exit(99);
    }
}

static inline void init_timeout_handler()
{
    struct sigaction sigact;

    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = timeout_handler;
    sigaction(SIGALRM, &sigact, NULL);
}

void RunnerFSM_::setup_launch_exec()
{
    m_exec = guest_config_.get_executable();

    // 1. setup m_exec_launch_dir
    if(m_sandbox_dir.empty())
    {
        // when no sandbox, m_exec_launch_dir is set to the parent folder of the executable,
        // unless that folder is not writable (then it will be the working
        // directory of crete-run)
        m_exec_launch_dir = m_exec.parent_path();
        if(access(m_exec_launch_dir.string().c_str(), W_OK) != 0)
        {
            m_exec_launch_dir = fs::current_path();
        }
    } else {
        m_exec_launch_dir = fs::path("/tmp") / fs::canonical(m_sandbox_dir).filename();
    }

    // 2. Set up m_launch_args
    config::Arguments guest_args = guest_config_.get_arguments();
    // +1 for argv[0]
    m_launch_args.resize(guest_args.size()+1, std::string());
    m_launch_args[0] = m_exec.string();

    for(config::Arguments::const_iterator it = guest_args.begin();
            it != guest_args.end(); ++it) {
        assert(it->index < m_launch_args.size());
        assert(m_launch_args[it->index].empty());
        m_launch_args[it->index] = it->value;
    }

    // 3. Setup m_launch_ctx
    // TODO: xxx the output stream may make a difference, check klee's symbolic_stdout
    m_launch_ctx.output_behavior.insert(bp::behavior_map::value_type(STDOUT_FILENO, bp::capture_stream()));
    m_launch_ctx.output_behavior.insert(bp::behavior_map::value_type(STDERR_FILENO, bp::redirect_stream_to_stdout()));
    m_launch_ctx.input_behavior.insert(bp::behavior_map::value_type(STDIN_FILENO, bp::capture_stream()));

    //TODO: xxx unified the environment, may use klee's env.sh
    m_launch_ctx.environment = bp::self::get_environment();
    m_launch_ctx.environment.erase("LD_PRELOAD");
    m_launch_ctx.environment.insert(bp::environment::value_type("LD_PRELOAD", "libcrete_run_preload.so"));

    m_launch_ctx.work_directory = m_exec_launch_dir.string();

    if(!m_sandbox_dir.empty())
    {
        m_launch_ctx.chroot = CRETE_SANDBOX_PATH;
    }

    // 4. setup the path for proc-map and guest_config_serialized
    if(m_sandbox_dir.empty())
    {
        m_proc_map = CRETE_PROC_MAPS_PATH;
        m_guest_config_serialized = CRETE_CONFIG_SERIALIZED_PATH;
    } else {
        m_proc_map = fs::path(CRETE_SANDBOX_PATH) / CRETE_PROC_MAPS_PATH;
        m_guest_config_serialized = fs::path(CRETE_SANDBOX_PATH) / CRETE_CONFIG_SERIALIZED_PATH;
    }

    // 5. setup timeout hanlder
    init_timeout_handler();
}

void RunnerFSM_::init_launch_folder()
{
    if(!m_sandbox_dir.empty())
    {
        init_sandbox();
    }

    //4. setup ramdisk
    init_ramdisk();
}

void RunnerFSM_::load_file_data(const poll&)
{
    guest_config_.load_file_data();
}

void RunnerFSM_::transmit_guest_data(const poll&)
{
    PacketInfo pk;
    pk.id = 0; // TODO: Don't care? Maybe. What about a custom instruction that reads the VM's pid as an ID, to be checked both for sanity and to give the instance a way to check whether the VM is still running.
    pk.size = 0; // Don't care. Set by write_serialized_text.
    pk.type = packet_type::guest_configuration;
    write_serialized_text_xml(*client_,
                              pk,
                              guest_config_);
}

void RunnerFSM_::prime(const poll&)
{
    setup_launch_exec();
    init_launch_folder();
    reset_sandbox();

    prime_executable();
    prime_virtual_machine();
}

void RunnerFSM_::prime_virtual_machine()
{
#if !defined(CRETE_TEST)
    crete_send_custom_instr_prime();
#endif // !defined(CRETE_TEST)
}

// Prime execute the exec (program) under test to get proc-map.log.
// The exec should exit prematurely within preload.so
void RunnerFSM_::prime_executable()
{
    fs::remove(m_proc_map);
    fs::remove(m_guest_config_serialized);

    guest_config_.is_first_iteration(true);
    write_configuration();

#if !defined(CRETE_TEST)
    // To get the proc-map.log
    launch_executable();
#endif // !defined(CRETE_TEST)
}

void RunnerFSM_::write_configuration() const
{
    std::ofstream ofs(m_guest_config_serialized.string().c_str());

    if(!ofs.good())
    {
        BOOST_THROW_EXCEPTION(Exception() << err::file_open_failed(m_guest_config_serialized.string()));
    }

    boost::archive::text_oarchive oa(ofs);
    oa << guest_config_;
}

static inline void process_exit_status(std::ostream& log, int exit_status)
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
            log << "[Signal Caught] signum = " << signum << ", signame: " << strsignal(signum) << std::endl;
        }
    }

    log << "ABNORMAL EXIT STATUS: " << exit_status << std::endl;
}

void RunnerFSM_::launch_executable()
{
#if defined(CRETE_DBG_SYSTEM_LAUNCH)
    // A alternative of bp::launch. require header file "#include <cstdlib>"
    std::string exec_cmd = "LD_PRELOAD=\"libcrete_run_preload.so\" ";
    for(vector<string>::const_iterator it = m_launch_args.begin();
            it != m_launch_args.end(); ++it) {
        exec_cmd = exec_cmd + (*it) + " ";
    }

    std::cerr << "Launch program with system(): " << exec_cmd << std::endl;

    // Note: Whether this function call is blocking or not depends on the shell being used
    std::system(exec_cmd.c_str());
#else
    bp::posix_child proc = bp::posix_launch(m_exec, m_launch_args, m_launch_ctx);

    pid_ = proc.get_id();
    monitored_pid = pid_;
    assert(monitored_timeout != 0);
    alarm(monitored_timeout);

    std::cerr << "=== Output from the target executable ===\n";
    bp::pistream& is = proc.get_stdout();
    std::string line;
    while(std::getline(is, line))
    {
        std::cerr << line << std::endl;
    }
    bp::status s = proc.wait();
    alarm(0);

    process_exit_status(std::cerr, s.exit_status());
    std::cerr << "=========================================\n";
#endif
}

void RunnerFSM_::signal_dump() const
{
#if !defined(CRETE_TEST)
    crete_send_custom_instr_dump();
#endif // !defined(CRETE_TEST)
}

void RunnerFSM_::process_config(const poll&)
{
#if !defined(CRETE_TEST)
    //FIXME: xxx should be disabled as CRETE now works with stripped binaries
    using namespace std;

    ProcReader proc_reader(m_guest_config_serialized);
    process_lib_filter(proc_reader,
                       guest_config_.get_libraries(),
                       crete_insert_instr_addr_include_filter);

    process_func_filter(proc_reader,
            guest_config_.get_include_functions(),
            crete_insert_instr_addr_include_filter);

    process_func_filter(proc_reader,
            guest_config_.get_exclude_functions(),
            crete_insert_instr_addr_exclude_filter);
#endif // !defined(CRETE_TEST)
}

void RunnerFSM_::validate_setup(const poll&)
{
#if !defined(CRETE_TEST)

#endif // !defined(CRETE_TEST)
}

void RunnerFSM_::update_config(const poll&)
{
    guest_config_.is_first_iteration(false);
    guest_config_.clear_file_data();

    write_configuration();

    is_first_exec_ = false;
}

void RunnerFSM_::execute(const next_test&)
{
    // Waiting for "next_test", blocking function
    // TODO: should waiting for the command to come in be a guard?
    PacketInfo pkinfo = client_->read();

    if(pkinfo.type != packet_type::cluster_next_test)
    {
        BOOST_THROW_EXCEPTION(Exception() << err::network_type_mismatch(pkinfo.type));
    }

#if !defined(CRETE_TEST)

    launch_executable();

#endif // !defined(CRETE_TEST)
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

// ramdisk folder: for symbolic file support
// equivalent cmd: "sudo mount -t tmpfs -o size=10M tmpfs DST"
void RunnerFSM_::init_ramdisk()
{
    const unsigned long mntflags = 0;
    const char* src  = "none";
    const char* type = "tmpfs";
    const char* opts = "size=10M";

    fs::path trgt;
    if(m_sandbox_dir.empty())
    {
        trgt = fs::path(CRETE_RAMDISK_PATH);
    } else {
        trgt = fs::path(CRETE_SANDBOX_PATH) / fs::path(CRETE_RAMDISK_PATH);
    }

    fs::create_directories(trgt);
    int mount_result = mount(src, trgt.string().c_str(), type, mntflags, opts);
    if(mount_result != 0)
    {
        fprintf(stderr, "[crete-run] mount failed: "
                        "src = %s, type = %s, opts = %s, trgt = %s\n",
                        src, type, opts, trgt.string().c_str());

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
void RunnerFSM_::reset_sandbox_folder_permission()
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
        fs::path p = fs::path(CRETE_SANDBOX_PATH) / m_exec_launch_dir;
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
void RunnerFSM_::init_sandbox()
{
    reset_sandbox_folder_permission();

    // delete the sandbox folder if it existed
    if(fs::exists(CRETE_SANDBOX_PATH))
    {
        {
            // umount potential mounted folders
            fs::path ramdisk_path = fs::path(CRETE_SANDBOX_PATH) / fs::path(CRETE_RAMDISK_PATH);
            if(fs::is_directory(ramdisk_path))
            {
                umount(ramdisk_path.string().c_str());
            }

            if(fs::is_directory(CRETE_SANDBOX_PATH))
            {
                for (fs::directory_iterator end_dir_it, it((fs::path(CRETE_SANDBOX_PATH))); it!=end_dir_it; ++it)
                {
                    if(fs::is_directory(it->path()))
                        umount(it->path().string().c_str());
                }
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
}

// reset CRETE_SANDBOX_EXEC folder by copying m_sandbox_dir
void RunnerFSM_::reset_sandbox()
{
    // 1. reset "ramdisk folder" within sandbox
    fs::path ramdisk_folder_path = fs::path(CRETE_SANDBOX_PATH) / CRETE_RAMDISK_PATH;
    assert(fs::is_directory(ramdisk_folder_path) && "[crete-run] ramdisk folder does not exsit. "
            "It should be created by \"init_sandbox()\"\n");
    for (fs::directory_iterator end_dir_it, it(ramdisk_folder_path); it!=end_dir_it; ++it)
    {
        fs::remove_all(it->path());
    }

    // 2. reset "sandbox-exec folder" within sandbox
    fs::path crete_sandbox_exec_path = fs::path(CRETE_SANDBOX_PATH) / m_exec_launch_dir;
    if(fs::exists(crete_sandbox_exec_path))
    {
        fs::remove_all(crete_sandbox_exec_path);
    }
    assert(fs::is_directory(fs::path(crete_sandbox_exec_path).parent_path()));

    bp::context ctx;
    ctx.stdout_behavior = bp::capture_stream();
    ctx.environment = bp::self::get_environment();

    std::string exec = bp::find_executable_in_path("cp");
    std::vector<std::string> args;
    args.push_back(exec);
    args.push_back("-r");
    args.push_back(m_sandbox_dir.string());
    args.push_back(crete_sandbox_exec_path.string());

    bp::child c = bp::launch(exec, args, ctx);

    bp::pistream &is = c.get_stdout();

    // TODO: xxx should check the return status to make sure the "cp" completed successfully
    bp::status s = c.wait();
}

void RunnerFSM_::finished(const poll&)
{
    signal_dump();

    pid_ = -1;
}

void RunnerFSM_::verify_invariants(const poll&)
{
#if !defined(CRETE_TEST)

    std::ifstream ifs (m_proc_map.string().c_str());
    std::string contents((
        std::istreambuf_iterator<char>(ifs)),
        std::istreambuf_iterator<char>());

    boost::hash<std::string> hash_fn;

    std::size_t new_hash = hash_fn(contents);

    assert(new_hash);

    if(proc_maps_hash_ == 0)
    {
        proc_maps_hash_ = new_hash;
    }
    else if(new_hash != proc_maps_hash_)
    {
        throw std::runtime_error("proc-maps.log changed across iterations! Ensure ASLR is disabled");
    }

    if(!m_sandbox_dir.empty())
    {
        init_launch_folder();
        //FIXME: xxx create dummy "proc-map.log" to prevent executable from terminating prematurely
        std::ofstream ofs (m_proc_map.string().c_str());
        ofs.close();
        // Provide "guest_config"
        write_configuration();

        reset_sandbox();
    }
#endif // !defined(CRETE_TEST)
}


void RunnerFSM_::process_func_filter(ProcReader& pr,
                                     const config::Functions& funcs,
                                     void (*f_custom_instr)(uintptr_t, uintptr_t))
{
    for(config::Functions::const_iterator it = funcs.begin();
        it != funcs.end();
        ++it)
    {
        if(it->lib.empty()) // Target: executable.
        {
            try
            {
                ELFReader reader(guest_config_.get_executable());

                Entry entry = reader.get_section_entry(".symtab", it->name.c_str());

                if(entry.addr == 0)
                {
                    std::cerr
                    << "[CRETE] Warning - failed to get address of '"
                    << it->name
                    << "' - ensure binary has a symbol table '.symtab'\n";
                }

                f_custom_instr(entry.addr, entry.addr + entry.size);
            }
            catch(std::exception& e)
            {
                std::cerr << "[CRETE WARNING]: ELFReader failed to process the executable. Likely caused by missing symbol table: "
                          << e.what();
            }
        }
        else // Target: library.
        {
            fs::path lib = deduce_library(it->lib, pr);
            std::vector<ProcMap> pms = pr.find(lib.generic_string());
            assert(pms.size() != 0);

            ELFReader ler(lib); // TODO: inefficient. Re-reading the ELF for each function entry.

            Entry entry = ler.get_section_entry(".symtab", it->name);

            uint64_t base_addr = pms.front().address().first;
            uint64_t addr = base_addr + entry.addr;

            f_custom_instr(addr, addr + entry.size);
        }
    }
}

// TODO: belongs as a free function in a utility library.
fs::path RunnerFSM_::deduce_library(const fs::path& lib,
                                    const ProcReader& pr)
{
    std::vector<ProcMap> pms = pr.find_all();

    boost::filesystem::path match;

    for(std::vector<ProcMap>::const_iterator it = pms.begin();
        it != pms.end();
        ++it)
    {
        fs::path test_path = it->path();
        if(lib == test_path)
        {
            return test_path;
        }
        else if(test_path.filename() == lib)
        {
            if(!match.empty() && match != test_path) // Same name, but different absolute path.
            {
                throw std::runtime_error("ambiguous libraries found for: " + lib.generic_string());
            }

            match = test_path;
        }
    }

    if(match.empty())
    {
        throw std::runtime_error("unable to find or deduce library: " + lib.generic_string());
    }

    return match;
}

void RunnerFSM_::process_lib_filter(ProcReader& pr,
                                const std::vector<std::string>& libs,
                                void (*f_custom_instr)(uintptr_t, uintptr_t))
{
    using namespace std;

    for(vector<string>::const_iterator iter = libs.begin();
        iter != libs.end();
        ++iter)
    {
        fs::path lib = deduce_library(*iter, pr);
        vector<ProcMap> pms = pr.find(lib.string());

        if(pms.empty())
            throw runtime_error("failed to get address of '" + *iter + "' library for filtering");

        for(vector<ProcMap>::iterator pmiter = pms.begin();
            pmiter != pms.end();
            ++pmiter)
        {
            f_custom_instr(pmiter->address().first, pmiter->address().second);
        }
    }
}

void RunnerFSM_::process_executable_section(ELFReader& reader,
                                            const std::vector<std::string>& sections,
                                            void (*f_custom_instr)(uintptr_t, uintptr_t))
{
    for(std::vector<std::string>::const_iterator iter = sections.begin();
        iter != sections.end();
        ++iter)
    {
        Entry entry = reader.get_section(*iter);

        if(entry.addr == 0)
            continue;//throw std::runtime_error("failed to get address of '" + *iter + "' - ensure binary has section");

        f_custom_instr(entry.addr, entry.addr + entry.size);
    }
}

void RunnerFSM_::process_library_sections(const ProcReader& pr)
{
#if !defined(CRETE_TEST)

    using namespace std;

    const vector<ProcMap> proc_maps = pr.find_all();

    set<string> lib_paths;
    for(vector<ProcMap>::const_iterator it = proc_maps.begin();
        it != proc_maps.end();
        ++it)
    {
        if(fs::exists(it->path()))
        {
            lib_paths.insert(it->path());
        }
    }

    for(set<string>::const_iterator it = lib_paths.begin();
        it != lib_paths.end();
        ++it)
    {
        string path = *it;

        vector<ProcMap> pms = pr.find(path);

        ELFReader ereader(path);

        uint64_t base_addr = pms.front().address().first;

        process_library_section(ereader,
                                guest_config_.get_section_exclusions(),
                                crete_insert_instr_call_stack_exclude,
                                base_addr);
    }

#endif // !defined(CRETE_TEST)
}

void RunnerFSM_::process_library_section(ELFReader& reader,
                                         const std::vector<std::string>& sections,
                                         void (*f_custom_instr)(uintptr_t, uintptr_t),
                                         uint64_t base_addr)
{
    for(std::vector<std::string>::const_iterator iter = sections.begin();
        iter != sections.end();
        ++iter)
    {
        Entry entry = reader.get_section(*iter);

        if(entry.addr == 0)
            continue;//throw std::runtime_error("failed to get address of '" + *iter + "' - ensure binary has section");

        f_custom_instr(entry.addr + base_addr,
                       entry.addr + base_addr + entry.size);
    }
}

bool RunnerFSM_::is_process_finished(const poll&)
{
    return !process::is_running(pid_);
}

bool RunnerFSM_::is_first_exec(const poll&)
{
    return is_first_exec_;
}

bool RunnerFSM_::is_not_first_exec(const poll&)
{
    return !is_first_exec_;
}

Runner::Runner(int argc, char* argv[]) :
    ops_descr_(make_options()),
    fsm_(boost::make_shared<RunnerFSM>()),
    stopped_(false)
{
    parse_options(argc, argv);
    process_options();

    start_FSM();

    run();
}

po::options_description Runner::make_options()
{
    po::options_description desc("Options");

    desc.add_options()
            ("help,h", "displays help message")
            ("config,c", po::value<fs::path>(), "configuration file")
            ("ip,i", po::value<std::string>(), "host IP")
            ("sandbox,s", po::value<fs::path>(), "sandbox directory")
        ;

    return desc;
}

void Runner::parse_options(int argc, char* argv[])
{
    po::store(po::parse_command_line(argc, argv, ops_descr_), var_map_);
    po::notify(var_map_);
}

void Runner::process_options()
{
    using namespace std;

//    if(var_map_.size() == 0)
//    {
//        cout << "Missing arguments" << endl;
//        cout << "Use '--help' for more details" << endl;

//        BOOST_THROW_EXCEPTION(Exception() << err::arg_count(0));
//    }
    if(var_map_.count("help"))
    {
        cout << ops_descr_ << endl;

        throw Exception();
    }
    if(var_map_.count("ip"))
    {
        ip_ = var_map_["ip"].as<std::string>();
    }
    else // Default IP
    {
        ip_ = "10.0.2.2";
    }
    if(var_map_.count("config"))
    {
        fs::path p = var_map_["config"].as<fs::path>();

        if(!fs::exists(p))
        {
            BOOST_THROW_EXCEPTION(Exception() << err::file_missing(p.string()));
        }

        target_config_ = p;
    }

    if(var_map_.count("sandbox"))
    {
        fs::path p = var_map_["sandbox"].as<fs::path>();

        if(!fs::exists(p) && !fs::is_directory(p))
        {
            BOOST_THROW_EXCEPTION(Exception() << err::file_missing(p.string()));
        }

        sandbox_dir_ = p;
    }
}

void Runner::start_FSM()
{
    fsm_->start();
}

void Runner::stop()
{
    stopped_ = true;
}

void Runner::run()
{
    start s(ip_,
            target_config_,
            sandbox_dir_);

    fsm_->process_event(s);

    while(!stopped_)
    {
        if(fsm_->is_flag_active<flag::next_test>())
        {
            fsm_->process_event(next_test());
        }
        else
        {
            fsm_->process_event(poll());
        }
    }
}

} // namespace crete
