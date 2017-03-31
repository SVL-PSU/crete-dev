#include <crete/cluster/svm_node_fsm.h>
#include <crete/cluster/common.h>
#include <crete/cluster/dispatch_options.h>
#include <crete/cluster/svm_node_options.h>
#include <crete/exception.h>
#include <crete/process.h>
#include <crete/asio/server.h>
#include <crete/run_config.h>
#include <crete/serialize.h>
#include <crete/async_task.h>
#include <crete/logger.h>
#include <crete/util/debug.h>
#include <crete/common.h>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/range/irange.hpp>
#include <boost/msm/back/state_machine.hpp> // back-end
#include <boost/msm/front/state_machine_def.hpp> //front-end
#include <boost/msm/front/functor_row.hpp>
#include <boost/thread.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/range/algorithm/remove_if.hpp>
#include <boost/range/algorithm/replace_if.hpp>

#include <boost/process.hpp>

#include <memory>

namespace bp = boost::process;
namespace fs = boost::filesystem;
namespace msm = boost::msm;
namespace mpl = boost::mpl;
namespace pt = boost::property_tree;
namespace bui = boost::uuids;
using namespace msm::front;

namespace crete
{
namespace cluster
{
namespace node
{
namespace svm
{

const auto klee_dir_name = std::string{"klee-run"};
const auto concolic_log_name = std::string{"concolic.log"};
const auto symbolic_log_name = std::string{"klee-run.log"};

// +--------------------------------------------------+
// + Exceptions                                       +
// +--------------------------------------------------+
struct SVMException : public Exception {};
struct ConcolicExecException : public SVMException {};
struct SymbolicExecException : public SVMException
{
    SymbolicExecException(const std::vector<TestCase>& tests)
        : tests_{tests}
    {}

    std::vector<TestCase> tests_;
};

// +--------------------------------------------------+
// + Events                                           +
// +--------------------------------------------------+

namespace ev
{

struct start // Basically, serves as constructor.
{
    start(const std::string& svm_dir
         ,const cluster::option::Dispatch& dispatch_options
         ,node::option::SVMNode& node_options)
        : svm_dir_{svm_dir}
        , dispatch_options_(dispatch_options)
        , node_options_{node_options} {}

    std::string svm_dir_;
    cluster::option::Dispatch dispatch_options_;
    node::option::SVMNode node_options_;
};

struct next_trace
{
    next_trace(const fs::path& trace) :
        trace_(trace)
    {}

    const fs::path& trace_;
};

struct connect
{
    connect(Port master_port) :
        master_port_(master_port)
    {}

    Port master_port_;
};

} // namespace ev

namespace fsm
{
// +--------------------------------------------------+
// + Finite State Machine                             +
// +--------------------------------------------------+

// +--------------------------------------------------+
// + State Machine Front End                          +
// +--------------------------------------------------+
class KleeFSM_ : public boost::msm::front::state_machine_def<KleeFSM_>
{
public:
    KleeFSM_();

    auto tests() -> std::vector<TestCase>;
    auto error() -> const log::NodeError&;

    // +--------------------------------------------------+
    // + Entry & Exit                                     +
    // +--------------------------------------------------+
    template <class Event,class FSM>
    void on_entry(Event const&,FSM&)
    {
        std::cerr << "entering: KleeFSM_" << std::endl;
    }
    template <class Event,class FSM>
    void on_exit(Event const&,FSM&)
    {
        std::cerr << "leaving: KleeFSM_" << std::endl;
    }

    // +--------------------------------------------------+
    // + States                                           +
    // +--------------------------------------------------+
    struct Start;
    struct NextTrace;
    struct Prepare;
    struct Prepared;
    struct ExecuteSymbolic;
    struct StoreTests;
    struct Terminated2;
    struct Finished;
    struct ResultReady;

    struct Active;
    struct Terminated;

    struct Valid;
    struct Error;

    // +--------------------------------------------------+
    // + Actions                                          +
    // +--------------------------------------------------+
    struct init;
    struct next_trace;
    struct prepare;
    struct execute_symbolic;
    struct retrieve_result;
    struct clean;
    struct terminate;
    struct terminate2;

    // +--------------------------------------------------+
    // + Gaurds                                           +
    // +--------------------------------------------------+
    struct is_prev_task_finished;

    // +--------------------------------------------------+
    // + Transitions                                      +
    // +--------------------------------------------------+
    template <class FSM,class Event>
    void exception_caught(Event const&,FSM& fsm,std::exception& e);

    // Initial state of the FSM.
    using initial_state = mpl::vector<Start, Active, Valid>;

    struct transition_table : mpl::vector
    <
    //    Start              Event              Target             Action                Guard
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Start             ,ev::start         ,NextTrace         ,init                 ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<NextTrace         ,ev::next_trace    ,Prepare           ,next_trace           ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Prepare           ,ev::poll          ,ExecuteSymbolic   ,prepare              ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<ExecuteSymbolic   ,ev::poll          ,StoreTests        ,execute_symbolic     ,is_prev_task_finished>,
      Row<ExecuteSymbolic   ,ev::terminate2    ,Terminated2       ,terminate2           ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<StoreTests        ,ev::poll          ,Finished          ,retrieve_result      ,is_prev_task_finished>,
      Row<StoreTests        ,ev::terminate2    ,Terminated2       ,terminate2           ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Finished          ,ev::poll          ,ResultReady       ,none                 ,is_prev_task_finished>,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<ResultReady       ,ev::tests_queued  ,NextTrace         ,clean                ,none                 >,
    // -- Orthogonal Region
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Active            ,ev::terminate     ,Terminated        ,terminate            ,none                 >,
    // -- Orthogonal Region
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Valid             ,ev::error         ,Error             ,none                 ,none                 >
    > {};

private:
    cluster::option::Dispatch dispatch_options_;
    node::option::SVMNode node_options_;
    fs::path svm_dir_;
    fs::path trace_dir_;
    std::shared_ptr<std::vector<TestCase>> tests_ = std::make_shared<std::vector<TestCase>>();
    crete::log::Logger exception_log_;
    log::NodeError error_log_;
    std::shared_ptr<AtomicGuard<pid_t> > translator_child_pid_ = std::make_shared<AtomicGuard<pid_t> >(-1);
    std::shared_ptr<AtomicGuard<pid_t> > klee_child_pid_ = std::make_shared<AtomicGuard<pid_t> >(-1);
};

template <class FSM,class Event>
void KleeFSM_::exception_caught(Event const&,FSM& fsm,std::exception& e)
{
    auto except_info = boost::diagnostic_information(e);
    std::cerr << "exception_caught" << std::endl;
    std::cerr << except_info << std::endl;
    exception_log_ << except_info;

    std::stringstream ss;

    ss << "Exception Caught:\n"
       << except_info
       << "Node: SVM\n"
       << "Trace: " << fsm.trace_dir_ << "\n"
       << "\n";
    // TODO: I'd also like to send the current state's ID (ideally, even the recent state history), but that requires some work: http://stackoverflow.com/questions/14166800/boostmsm-a-way-to-get-a-string-representation-ie-getname-of-a-state
    // Cont: I could ad hoc it by keeping a history_ variable that I append to for each state, and condense to the last N.
    // Cont: history_ could be stored using boost::circular_buffer, or std::vector with a mod operator, to keep it's size down to N.

    if(dynamic_cast<Exception*>(&e))
    {
        if(dynamic_cast<SVMException*>(&e))
        {
            auto dump_log_file = [](std::stringstream& ss,
                                    const fs::path& log_file)
            {
                fs::ifstream ifs(log_file);

                if(!ifs.good())
                {
                    ss << "Failed to open associated log file: "
                       << log_file.filename().string();
                }
                else
                {
                    ss << "Log File ["
                       << log_file.filename().string()
                       << "]:\n"
                       << ifs.rdbuf()
                       << '\n';
                }
            };

            if(dynamic_cast<ConcolicExecException*>(&e))
            {
                dump_log_file(ss, fsm.trace_dir_ / klee_dir_name / concolic_log_name);
            }
            else if(dynamic_cast<SymbolicExecException*>(&e))
            {
                auto* se = dynamic_cast<SymbolicExecException*>(&e);

                fsm.tests_->clear();
                if(!se->tests_.empty())
                {
                    // Retrieve the base test case first;
                    fsm.tests_->push_back(retrieve_test_serialized((fsm.trace_dir_ / "concrete_inputs.bin").string()));
                    fsm.tests_->insert(fsm.tests_->end(), se->tests_.begin(), se->tests_.end());
                }

                dump_log_file(ss, fsm.trace_dir_ / klee_dir_name / symbolic_log_name);
            }
        }
    }
    else
    {
        // TODO: handle more gracefully...
        {
            // TODO: use constant for 'svm' path.
            fs::ofstream fatal_ofs(fs::path{"svm"} / "fatal_error.log");

            if(!fatal_ofs.good())
            {
                assert(0 && "Failed to open log file");
            }

            fatal_ofs << ss.rdbuf();
        }

        assert(0 && "Unknown exception");
//        fsm.process_event(ev::terminate{});
    }

    error_log_.log = ss.str();
    fsm.process_event(ev::error{});
}

// +--------------------------------------------------+
// + State Machine Front End                          +
// +--------------------------------------------------+
inline
KleeFSM_::KleeFSM_()
{
}

inline
auto KleeFSM_::tests() -> std::vector<TestCase>
{
    return *tests_;
}

inline
auto KleeFSM_::error() -> const log::NodeError&
{
    return error_log_;
}

// +--------------------------------------------------+
// + States                                           +
// +--------------------------------------------------+
struct KleeFSM_::Start : public msm::front::state<>
{
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cerr << "entering: Start" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cerr << "leaving: Start" << std::endl;}
};
struct KleeFSM_::NextTrace : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::next_trace>;

    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cerr << "entering: NextTrace" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cerr << "leaving: NextTrace" << std::endl;}
};
struct KleeFSM_::Prepare : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::active>;

    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cerr << "entering: Prepare" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cerr << "leaving: Prepare" << std::endl;}

    std::unique_ptr<AsyncTask> async_task_;
};
struct KleeFSM_::ExecuteSymbolic : public msm::front::state<>
{
    using flag_list = mpl::vector2<flag::active, flag::active_async_task>;

    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cerr << "entering: ExecuteSymbolic" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cerr << "leaving: ExecuteSymbolic" << std::endl;}

    std::unique_ptr<AsyncTask> async_task_;
};
struct KleeFSM_::StoreTests : public msm::front::state<>
{
    using flag_list = mpl::vector2<flag::active, flag::active_async_task>;

    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cerr << "entering: StoreTests" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cerr << "leaving: StoreTests" << std::endl;}

    std::unique_ptr<AsyncTask> async_task_;
};
struct KleeFSM_::Terminated2: public msm::front::state<>
{
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cerr << "entering: Terminated2" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cerr << "leaving: Terminated2" << std::endl;}
};
struct KleeFSM_::Finished : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::active>;

    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cerr << "entering: Finished" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cerr << "leaving: Finished" << std::endl;}

    std::unique_ptr<AsyncTask> async_task_;
};
struct KleeFSM_::ResultReady : public msm::front::state<>
{
    using flag_list = mpl::vector2<flag::tests_ready, flag::active>;

    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cerr << "entering: ResultReady" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cerr << "leaving: ResultReady" << std::endl;}
};
struct KleeFSM_::Active : public msm::front::state<>
{
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cerr << "entering: Active" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cerr << "leaving: Active" << std::endl;}
};
struct KleeFSM_::Terminated : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::terminated>;

    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cerr << "entering: Terminated" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cerr << "leaving: Terminated" << std::endl;}
};
struct KleeFSM_::Valid : public msm::front::state<>
{
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cerr << "entering: Valid" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cerr << "leaving: Valid" << std::endl;}
};
struct KleeFSM_::Error : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::error>;

    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cerr << "entering: Error" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cerr << "leaving: Error" << std::endl;}
};

// +--------------------------------------------------+
// + Actions                                          +
// +--------------------------------------------------+
struct KleeFSM_::init
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&, TargetState&) -> void
    {
        fsm.svm_dir_ = ev.svm_dir_;
        fsm.dispatch_options_ = ev.dispatch_options_;
        fsm.node_options_ = ev.node_options_;

        if(!fs::exists(fsm.svm_dir_))
        {
            fs::create_directories(fsm.svm_dir_);
        }

        fsm.exception_log_.add_sink(fsm.svm_dir_ / log_dir_name / exception_log_file_name);
        fsm.exception_log_.auto_flush(true);
    }
};

struct KleeFSM_::next_trace
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&, TargetState& ts) -> void
    {
        fsm.trace_dir_ = fsm.svm_dir_ / ev.trace_.filename().string();
    }
};

struct KleeFSM_::prepare
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState& ts) -> void
    {
        ts.async_task_.reset(new AsyncTask{[](fs::path trace_dir
                                             ,cluster::option::Dispatch dispatch_options
                                             ,option::SVMNode node_options
                                             ,std::shared_ptr<AtomicGuard<pid_t>> child_pid)
        {
            fs::path dir = trace_dir;
            fs::path kdir = dir / klee_dir_name;

            if(!fs::exists(dir))
            {
                BOOST_THROW_EXCEPTION(SVMException{} << err::file_missing{dir.string()});
            }

            // 1. Translate qemu-ir to llvm
            bp::context ctx;
            ctx.work_directory = dir.string();
            ctx.environment = bp::self::get_environment();
            ctx.stdout_behavior = bp::capture_stream();
            ctx.stderr_behavior = bp::redirect_stream_to_stdout();

            {
                auto exe = std::string{};

                if(dispatch_options.vm.arch == "x86")
                {
                    if(!node_options.translator.path.x86.empty())
                    {
                        exe = node_options.translator.path.x86;
                    }
                    else
                    {
                        exe = bp::find_executable_in_path("crete-llvm-translator-qemu-2.3-i386");
                    }
                }
                else if(dispatch_options.vm.arch == "x64")
                {
                    if(!node_options.translator.path.x64.empty())
                    {
                        exe = node_options.translator.path.x64;
                    }
                    else
                    {
                        exe = bp::find_executable_in_path("crete-llvm-translator-qemu-2.3-x86_64");
                    }
                }
                else
                {
                    BOOST_THROW_EXCEPTION(Exception{} << err::arg_invalid_str{dispatch_options.vm.arch}
                                                      << err::arg_invalid_str{"vm.arch"});
                }

                auto args = std::vector<std::string>{fs::absolute(exe).string()}; // It appears our modified QEMU requires full path in argv[0]...

                auto proc = bp::launch(exe, args, ctx);

                child_pid->acquire() = proc.get_id();

                // TODO: xxx Work-around to resolve the deadlock happened within the child process
                // when its output is redirected.
                auto& pistream = proc.get_stdout();
                std::stringstream ss;
                std::string line;

                while(std::getline(pistream, line))
                    ss << line;

                auto status = proc.wait();

                // FIXME: xxx Between 'auto status = proc.wait();' and this statement,
                //           there is a chance this pid is reclaimed by other process.
                child_pid->acquire() = -1;

                if(!process::is_exit_status_zero(status))
                {
                    BOOST_THROW_EXCEPTION(SVMException{} << err::process_exit_status{exe}
                                                         << err::msg{ss.str()});
                }

                fs::rename(dir / "dump_llvm_offline.bc",
                           dir / "run.bc");

//                fs::remove(dir / "dump_tcg_llvm_offline.*.bin");
                for( fs::directory_iterator dir_iter(dir), end_iter ; dir_iter != end_iter ; ++dir_iter)
                {
                    std::string filename = dir_iter->path().filename().string();
                    if(filename.find("dump_tcg_llvm_offline") != std::string::npos)
                        fs::remove(dir/filename);
                }
            }

            // 2. copy files into kdir
            if(!fs::exists(kdir))
            {
                fs::create_directory(kdir);
            }

            auto copy_files = std::vector<std::string>{
                                                       "concrete_inputs.bin",
                                                       "run.bc"
                                                      };

            for( fs::directory_iterator dir_iter(dir), end_iter ; dir_iter != end_iter ; ++dir_iter)
            {
                std::string filename = dir_iter->path().filename().string();
                if(filename.find("dump_") != std::string::npos)
                    fs::copy_file(dir/filename, kdir/filename);
            }

            for(const auto f : copy_files)
            {
                fs::copy_file(dir/f, kdir/f);
            }
        }
        , fsm.trace_dir_
        , fsm.dispatch_options_
        , fsm.node_options_
        , fsm.translator_child_pid_});
    }
};

static bool is_klee_log_correct(const boost::filesystem::path& file)
{
    boost::filesystem::ifstream ifs(file);

    if(!ifs)
    {
        BOOST_THROW_EXCEPTION(Exception() << err::file_open_failed(file.string()));
    }

    boost::regex valid_patterns("KLEE: output directory .*klee-out-0\"|"
                                "(KLEE: done: total instructions = )[0-9]+|"
                                "(KLEE: done: completed paths = )[0-9]+|"
                                "(KLEE: done: generated tests = )[0-9]+"
                                );
    bool  passed = true;
    std::string line;
    while(std::getline(ifs, line))
      {
        if(line.empty())
          continue;

        if(!boost::regex_match(line, valid_patterns))
            return false ;
      }

    return true;
}

struct KleeFSM_::execute_symbolic
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState& ts) -> void
    {
        ts.async_task_.reset(new AsyncTask{[](fs::path trace_dir
                                             ,cluster::option::Dispatch dispatch_options
                                             ,option::SVMNode node_options
                                             ,std::shared_ptr<AtomicGuard<pid_t>> child_pid)
        {
            auto kdir = trace_dir / klee_dir_name;

            bp::context ctx;
            ctx.work_directory = kdir.string();
            ctx.environment = bp::self::get_environment();
            ctx.stdout_behavior = bp::capture_stream();
            ctx.stderr_behavior = bp::redirect_stream_to_stdout();

            auto exe = std::string{};

            if(!node_options.svm.path.symbolic.empty())
            {
                exe = node_options.svm.path.symbolic;
            }
            else
            {
                exe = bp::find_executable_in_path("crete-klee");
            }

            auto args = std::vector<std::string>{fs::path{exe}.filename().string()};

            auto add_args = std::vector<std::string>{};

            boost::split(add_args
                        ,dispatch_options.svm.args.symbolic
                        ,boost::is_any_of(" \t\n"));

            add_args.erase(std::remove_if(add_args.begin(),
                                          add_args.end(),
                                          [](const std::string& s)
                                          {
                                              return s.empty();
                                          }),
                           add_args.end());

            args.insert(args.end()
                       ,add_args.begin()
                       ,add_args.end());

            args.emplace_back("run.bc");

            for(auto& e : args)
                std::cerr << e << std::endl;

            auto proc = bp::launch(exe, args, ctx);

            child_pid->acquire() = proc.get_id();

            auto log_path = kdir / symbolic_log_name;

            {
                fs::ofstream ofs(log_path);
                if(!ofs.good())
                {
                    BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{log_path.string()});
                }

                bp::pistream& is = proc.get_stdout();
                std::string line;
                while(std::getline(is, line))
                {
                    ofs << line << '\n';
                }
            }

            auto status = proc.wait();

            // FIXME: xxx Between 'auto status = proc.wait();' and this statement,
            //           there is a chance this pid is reclaimed by other process.
            child_pid->acquire() = -1;

            if(!process::is_exit_status_zero(status))
            {
                BOOST_THROW_EXCEPTION(SymbolicExecException{retrieve_tests_serialized((kdir / std::string(CRETE_SVM_TEST_FOLDER)).string())} << err::process_exit_status{exe});
            }

            if(!is_klee_log_correct(log_path))
            {
                BOOST_THROW_EXCEPTION(SymbolicExecException{retrieve_tests_serialized((kdir / std::string(CRETE_SVM_TEST_FOLDER)).string())} << err::process{exe});
            }
        }
        , fsm.trace_dir_
        , fsm.dispatch_options_
        , fsm.node_options_
        , fsm.klee_child_pid_});
    }
};

struct KleeFSM_::retrieve_result
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState& ts) -> void
    {
        ts.async_task_.reset(new AsyncTask{[](fs::path trace_dir,
                                              std::shared_ptr<std::vector<TestCase>> tests)
        {
            auto kdir = trace_dir / klee_dir_name;

            std::vector<TestCase> tmp_tsts = retrieve_tests_serialized((kdir / std::string(CRETE_SVM_TEST_FOLDER)).string());

            tests->clear();
            if(!tmp_tsts.empty())
            {
                // Retrieve the base test case at first
                tests->push_back(retrieve_test_serialized((trace_dir / "concrete_inputs.bin").string()));
                tests->insert(tests->end(), tmp_tsts.begin(), tmp_tsts.end());
            }

        }, fsm.trace_dir_, fsm.tests_});
    }
};

struct KleeFSM_::clean
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        fsm.tests_->clear();

        if(fs::remove_all(fsm.trace_dir_) == 0)
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::file{"failed to remove trace directory"});
        }
    }
};


struct KleeFSM_::terminate2
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState& ss, TargetState&) -> void
    {
        std::cerr << "KleeFSM_::terminate2() entered\n";

        while(!ss.async_task_->is_finished());

        if(ss.async_task_->is_exception_thrown())
        {
            std::cerr << "[KleeFSM_::terminate2] Releasing exception\n";
            ss.async_task_->release_exception();
        }

        std::cerr << "KleeFSM_::terminate2() finished\n";
    }
};

struct KleeFSM_::terminate
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        std::cerr << "KleeFSM_::terminate() entered\n";
        {
            assert(fsm.translator_child_pid_);
            auto lock = fsm.translator_child_pid_->acquire();
            auto pid = static_cast<pid_t>(lock);
            if(pid != -1 &&
                    process::is_running(pid))
            {
                if(::kill(pid, SIGKILL) != 0)
                {
                    BOOST_THROW_EXCEPTION(Exception{} << err::process{"failed to kill crete-translator instance"}
                    << err::process_error{pid}
                    << err::c_errno{errno});
                }

                while(process::is_running(pid)) {} // TODO: is this check necessary?
            };
        }

        {
            assert(fsm.klee_child_pid_);
            auto lock = fsm.klee_child_pid_->acquire();
            auto pid = static_cast<pid_t>(lock);
            if(pid != -1 &&
                    process::is_running(pid))
            {
                if(::kill(pid, SIGKILL) != 0)
                {
                    BOOST_THROW_EXCEPTION(Exception{} << err::process{"failed to kill crete-klee instance"}
                    << err::process_error{pid}
                    << err::c_errno{errno});
                }

                while(process::is_running(pid)) {} // TODO: is this check necessary?
            };
        }

        {
            // TODO: xxx better way is to use visitor pattern
            if(fsm.template is_flag_active<flag::active_async_task>())
            {
                std::cerr << "KleeFSM_::active_async_task() ative\n";
                fsm.process_event(ev::terminate2());
            }
        }
        std::cerr << "KleeFSM_::terminate() finished\n";
    }
};

// +--------------------------------------------------+
// + Gaurds                                           +
// +--------------------------------------------------+
struct KleeFSM_::is_prev_task_finished
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM&, SourceState& ss, TargetState&) -> bool
    {
        if(ss.async_task_->is_exception_thrown())
        {
            auto e = ss.async_task_->release_exception();

            std::rethrow_exception(e);
        }

        return ss.async_task_->is_finished();
    }
};


// Normally would just: "using QemuFSM = boost::msm::back::state_machine<KleeFSM_>;",
// but needed a way to hide the impl. in source file, so as to avoid namespace troubles.
// This seems to work.
class KleeFSM : public boost::msm::back::state_machine<KleeFSM_>
{
};

} // namespace fsm
} // namespace svm
} // namespace node
} // namespace cluster
} // namespace crete
