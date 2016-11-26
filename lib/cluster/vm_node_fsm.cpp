#include <crete/cluster/vm_node_fsm.h>
#include <crete/cluster/common.h>
#include <crete/cluster/dispatch_options.h>
#include <crete/cluster/vm_node_options.h>
#include <crete/exception.h>
#include <crete/process.h>
#include <crete/asio/server.h>
#include <crete/run_config.h>
#include <crete/serialize.h>
#include <crete/async_task.h>
#include <crete/logger.h>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/range/irange.hpp>
#include <boost/msm/back/state_machine.hpp> // back-end
#include <boost/msm/front/state_machine_def.hpp> //front-end
#include <boost/msm/front/functor_row.hpp>
#include <boost/msm/front/euml/operator.hpp>
#include <boost/thread.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/uuid/uuid_generators.hpp>

#include <boost/property_tree/xml_parser.hpp>

#include <boost/process.hpp>

#include <memory>

#include <algorithm>

namespace bp = boost::process;
namespace fs = boost::filesystem;
namespace msm = boost::msm;
namespace mpl = boost::mpl;
namespace pt = boost::property_tree;
using namespace msm::front;
using namespace msm::front::euml;

namespace crete
{
namespace cluster
{
namespace node
{
namespace vm
{
// +--------------------------------------------------+
// + Exceptions                                       +
// +--------------------------------------------------+
struct VMException : public Exception {};

// +--------------------------------------------------+
// + Events                                           +
// +--------------------------------------------------+
namespace ev
{

struct start // Basically, serves as constructor.
{
    start(const cluster::option::Dispatch& dispatch_options,
          const node::option::VMNode& node_options,
          const fs::path& vm_dir)
           : start{dispatch_options,
                   node_options,
                   vm_dir,
                   "",
                   false,
                   "",
                   true} {}
    start(const cluster::option::Dispatch& dispatch_options,
          const node::option::VMNode& node_options,
          const fs::path& vm_dir,
          const fs::path& image_path,
          bool first_vm,
          const std::string& target,
          const bool clear_trace_folder)
        : dispatch_options_(dispatch_options) // TODO: seems to be an error in Clang 3.2 initializer syntax. Workaround: using parenthesese.
        , node_options_{node_options}
        , vm_dir_{vm_dir}
        , image_path_{image_path}
        , first_vm_{first_vm}
        , target_{target}
        , clear_trace_folder_(clear_trace_folder){}

    cluster::option::Dispatch dispatch_options_;
    node::option::VMNode node_options_;
    fs::path vm_dir_;
    fs::path image_path_;
    bool first_vm_;
    std::string target_;
    bool clear_trace_folder_;
};

struct next_test
{
    next_test(const TestCase& tc) :
        tc_(tc)
    {}

    TestCase tc_;
};

struct first
{
};

struct error
{
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
class QemuFSM_ : public boost::msm::front::state_machine_def<QemuFSM_>
{
public:
    QemuFSM_();
    ~QemuFSM_();

    auto trace() const -> const fs::path&;
    auto image_info() const -> ImageInfo;
    auto image_path() const -> fs::path;
    auto pwd() -> const fs::path&;
    auto guest_data() -> const GuestData&;
    auto initial_test() -> const TestCase&;
    auto error() -> const log::NodeError&;

    // +--------------------------------------------------+
    // + Entry & Exit                                     +
    // +--------------------------------------------------+
    template <class Event,class FSM>
    void on_entry(Event const&,FSM&)
    {
        std::cout << "entering: QemuFSM_" << std::endl;
    }
    template <class Event,class FSM>
    void on_exit(Event const&,FSM&)
    {
        std::cout << "leaving: QemuFSM_" << std::endl;
    }

    // +--------------------------------------------------+
    // + States                                           +
    // +--------------------------------------------------+
    struct Start;
    struct ValidateImage;
    struct UpdateImage;
    struct StartVM;
    struct RxGuestData;
    struct GuestDataRxed;
    struct NextTest;
    struct Testing;
    struct StoreTrace;
    struct Finished;
    struct ConnectVM;

    struct Active;
    struct Terminated;

    struct Valid;
    struct Error;

    // +--------------------------------------------------+
    // + Actions                                          +
    // +--------------------------------------------------+
    struct init;
    struct clean;
    struct read_vm_pid;
    struct update_image;
    struct start_vm;
    struct start_test;
    struct store_trace;
    struct report_error;
    struct connect_vm;
    struct receive_guest_info;
    struct finish;
    struct terminate;

    // +--------------------------------------------------+
    // + Gaurds                                           +
    // +--------------------------------------------------+
    struct is_prev_task_finished;
    struct is_image_valid;
    struct is_first_vm;
    struct is_finished;
    struct is_distributed;
    struct has_next_target;
    struct is_vm_terminated;

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
      Row<Start             ,ev::start         ,ValidateImage     ,ActionSequence_<mpl::vector<
                                                                       clean,
                                                                       init>>           ,is_distributed   >,
      Row<Start             ,ev::start         ,ConnectVM         ,ActionSequence_<mpl::vector<
                                                                       clean,
                                                                       init,
                                                                       read_vm_pid>>    ,Not_<is_distributed> >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<ValidateImage     ,ev::poll          ,UpdateImage       ,none                 ,Not_<is_image_valid> >,
      Row<ValidateImage     ,ev::poll          ,StartVM           ,none                 ,is_image_valid  >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<UpdateImage       ,ev::poll          ,StartVM           ,update_image         ,none            >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<StartVM           ,ev::poll          ,ConnectVM         ,start_vm             ,is_prev_task_finished>,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<ConnectVM         ,ev::poll          ,NextTest          ,connect_vm           ,And_<is_prev_task_finished,
                                                                                              And_<Not_<is_distributed>,
                                                                                                   Not_<is_first_vm>>> >,
      Row<ConnectVM         ,ev::poll          ,RxGuestData       ,connect_vm           ,And_<is_prev_task_finished,
                                                                                              And_<Not_<is_distributed>,
                                                                                                   is_first_vm>>    >,
      Row<ConnectVM         ,ev::poll          ,NextTest          ,connect_vm           ,And_<is_prev_task_finished,
                                                                                              And_<is_distributed,
                                                                                                   And_<has_next_target,
                                                                                                        Not_<is_first_vm>>>> >,
      Row<ConnectVM         ,ev::poll          ,RxGuestData       ,connect_vm           ,And_<is_prev_task_finished,
                                                                                              And_<is_distributed,
                                                                                                   And_<has_next_target,
                                                                                                        is_first_vm>>>    >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<RxGuestData       ,ev::poll          ,GuestDataRxed     ,receive_guest_info   ,is_prev_task_finished>,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<GuestDataRxed     ,ev::poll          ,NextTest          ,none                 ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<NextTest          ,ev::next_test     ,Testing           ,start_test           ,is_prev_task_finished>,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Testing           ,ev::poll          ,StoreTrace        ,store_trace          ,is_finished     >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<StoreTrace        ,ev::poll          ,Finished          ,none                 ,is_prev_task_finished>,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Finished          ,ev::trace_queued  ,NextTest          ,finish               ,none            >,
    // -- Orthogonal Region
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Active            ,ev::terminate     ,Terminated        ,terminate            ,none            >,
    // -- Orthogonal Region
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Valid             ,ev::error         ,Error             ,terminate            ,is_distributed  >,
      Row<Valid             ,ev::error         ,Terminated        ,none                 ,Not_<is_distributed> >
    > {};

private:
    cluster::option::Dispatch dispatch_options_;
    node::option::VMNode node_options_;
    fs::path vm_dir_;
    fs::path new_image_path_;
    boost::thread image_updater_thread_;
    std::shared_ptr<fs::path> trace_{std::make_shared<fs::path>()}; // To be read when is_flag_active<trace_ready>() == true.
    std::shared_ptr<AtomicGuard<bp::child>> child_{std::make_shared<AtomicGuard<bp::child>>(-1, bp::detail::file_handle(), bp::detail::file_handle(), bp::detail::file_handle())};
    std::shared_ptr<Server> server_{std::make_shared<Server>()}; // Ctor acquires unique port.
    bool first_vm_{false};
    GuestData guest_data_;
    TestCase initial_test_;
    boost::thread start_vm_thread_;
    std::string target_;
    crete::log::Logger exception_log_;
    log::NodeError error_log_;

    // Testing
    boost::thread qemu_stream_capture_thread_;
};

template <class FSM,class Event>
void QemuFSM_::exception_caught(Event const&,FSM& fsm,std::exception& e)
{
    std::cerr << "======================================================\n";

    auto except_info = boost::diagnostic_information(e);
    exception_log_ << except_info;

    std::stringstream ss;

    ss << "Exception Caught:\n"
       << except_info
       << "Node: VM\n"
       << "Target: " << fsm.target_ << "\n"
       << "VM dir: " << fsm.vm_dir_ << "\n";

    std::cerr << ss.str();

    // Get the concrete input if a VM exception happened
    static uint64_t count_vm_exception = 0;

    if(fs::exists(fsm.vm_dir_ / hostfile_dir_name / input_args_name)){
        std::stringstream crash_tc_ss;
        crash_tc_ss << "inputs_" << fs::path{fsm.target_}.filename().string()
                << "_crash_" << count_vm_exception++ << ".bin";
        fs::copy_file(fsm.vm_dir_ / hostfile_dir_name / input_args_name,
                fsm.vm_dir_ / hostfile_dir_name/ crash_tc_ss.str(),
                fs::copy_option::overwrite_if_exists);

        ss << crash_tc_ss.str() << " being dumped\n" << std::endl;

        std::cerr << crash_tc_ss.str() << " being dumped\n" << std::endl;
    } else {
        ss << (fsm.vm_dir_ / hostfile_dir_name / input_args_name) << " is missing. No concrete input is dumped.\n";

        std::cerr << (fsm.vm_dir_ / hostfile_dir_name / input_args_name) << " is missing. No concrete input is dumped.\n";
    }

    // TODO: I'd also like to send the current state's ID (ideally, even the recent state history), but that requires some work: http://stackoverflow.com/questions/14166800/boostmsm-a-way-to-get-a-string-representation-ie-getname-of-a-state
    // Cont: I could ad hoc it by keeping a history_ variable that I append to for each state, and condense to the last N.
    // Cont: history_ could be stored using boost::circular_buffer, or std::vector with a mod operator, to keep it's size down to N.

    if(dispatch_options_.mode.distributed)
    {
        ss << child_->acquire()->get_stdout().rdbuf();
    }

    error_log_.log = ss.str();

    if(dynamic_cast<VMException*>(&e))
    {
        std::cerr << "VMException thrown.\n";
        fsm.process_event(ev::error{});
    }
    else
    {
        std::cerr << "non VMException thrown.\n";
        fsm.process_event(ev::terminate{});
    }

    std::cerr << "======================================================\n";
}

// +--------------------------------------------------+
// + State Machine Front End                          +
// +--------------------------------------------------+
inline
QemuFSM_::QemuFSM_()
{
}

inline
QemuFSM_::~QemuFSM_()
{
    // TODO: kill pid_?
}

inline
auto QemuFSM_::trace() const -> const fs::path&
{
    return *trace_;
}

inline
auto QemuFSM_::image_info() const -> ImageInfo
{
    return ImageInfo{image_path()};
}

inline
auto QemuFSM_::image_path() const -> fs::path
{
    return fs::path{vm_dir_ / image_name};
}

inline
auto QemuFSM_::pwd() -> const fs::path&
{
    return vm_dir_;
}

inline
auto QemuFSM_::guest_data() -> const GuestData&
{
    return guest_data_;
}

inline
auto QemuFSM_::initial_test() -> const TestCase&
{
    return initial_test_;
}

inline
auto QemuFSM_::error() -> const log::NodeError&
{
    return error_log_;
}

// +--------------------------------------------------+
// + States                                           +
// +--------------------------------------------------+
struct QemuFSM_::Start : public msm::front::state<>
{
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Start" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Start" << std::endl;}
};
struct QemuFSM_::ValidateImage : public msm::front::state<>
{
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: ValidateImage" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: ValidateImage" << std::endl;}
};
struct QemuFSM_::UpdateImage : public msm::front::state<>
{
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: UpdateImage" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: UpdateImage" << std::endl;}
};
struct QemuFSM_::StartVM : public msm::front::state<>
{
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: StartVM" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: StartVM" << std::endl;}

    std::unique_ptr<AsyncTask> async_task_{new AsyncTask{}};
};
struct QemuFSM_::RxGuestData : public msm::front::state<>
{
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: RxGuestData" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: RxGuestData" << std::endl;}

    std::unique_ptr<AsyncTask> async_task_;
};
struct QemuFSM_::GuestDataRxed : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::guest_data_rxed>;

    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: GuestDataRxed" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: GuestDataRxed" << std::endl;}
};
struct QemuFSM_::NextTest : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::next_test>;

    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: NextTest" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: NextTest" << std::endl;}

    std::unique_ptr<AsyncTask> async_task_{new AsyncTask{}};
};
struct QemuFSM_::Testing : public msm::front::state<>
{
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Testing" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Testing" << std::endl;}
};
struct QemuFSM_::StoreTrace : public msm::front::state<>
{
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: StoreTrace" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: StoreTrace" << std::endl;}

    std::unique_ptr<AsyncTask> async_task_;
};
struct QemuFSM_::Finished : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::trace_ready>;

    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Finished" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Finished" << std::endl;}
};
struct QemuFSM_::ConnectVM : public msm::front::state<>
{
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: ConnectVM" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: ConnectVM" << std::endl;}

    std::unique_ptr<AsyncTask> async_task_{new AsyncTask{}};
};
struct QemuFSM_::Active : public msm::front::state<>
{
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Active" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Active" << std::endl;}
};
struct QemuFSM_::Terminated : public msm::front::terminate_state<>
{
    using flag_list = mpl::vector1<flag::terminated>;

    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Terminated" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Terminated" << std::endl;}
};
struct QemuFSM_::Valid : public msm::front::state<>
{
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Valid" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Valid" << std::endl;}
};
struct QemuFSM_::Error : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::error>;

    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Error" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Error" << std::endl;}
};
// +--------------------------------------------------+
// + Actions                                          +
// +--------------------------------------------------+

struct QemuFSM_::init
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&, TargetState&) -> void
    {
        fsm.vm_dir_ = ev.vm_dir_;
        fsm.dispatch_options_ = ev.dispatch_options_;
        fsm.node_options_ = ev.node_options_;
        fsm.new_image_path_ = ev.image_path_;
        fsm.first_vm_ = ev.first_vm_;
        fsm.target_ = ev.target_;

        fs::create_directories(fsm.vm_dir_);
        fs::create_directories(fsm.vm_dir_ / hostfile_dir_name);

        fsm.exception_log_.add_sink(fsm.vm_dir_ / log_dir_name / exception_log_file_name);
        fsm.exception_log_.auto_flush(true);
    }
};

struct QemuFSM_::clean
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM&, SourceState&, TargetState&) -> void
    {
        auto hostfile_dir = ev.vm_dir_ / hostfile_dir_name;

        if(ev.clear_trace_folder_)
        {
            fs::remove_all(ev.vm_dir_ / trace_dir_name);
        }

        fs::remove_all(ev.vm_dir_ / log_dir_name);
        fs::remove(hostfile_dir / input_args_name);
        fs::remove(hostfile_dir / trace_ready_name);

        if(ev.dispatch_options_.mode.distributed)
        {
            fs::remove(hostfile_dir / vm_pid_file_name);
        }
    }
};

struct QemuFSM_::read_vm_pid
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        auto p = fs::path{hostfile_dir_name} / vm_pid_file_name;

        if(!fs::exists(p))
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::file_missing{p.string()});
        }

        fs::ifstream ifs(p,
                         std::ios::binary | std::ios::in);

        if(!ifs.good())
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{p.string()});
        }

        auto pid = pid_t{-1};

        ifs >> pid;

        fsm.child_->acquire() = bp::child{pid
                                         ,bp::detail::file_handle{}
                                         ,bp::detail::file_handle{}
                                         ,bp::detail::file_handle{}};
    }
};

struct QemuFSM_::update_image
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState& ts) -> void
    {
        ts.async_task_.reset(new AsyncTask{[](const fs::path new_image,
                                              const fs::path old_image)
        {
            fs::copy_file(new_image,
                          old_image,
                          fs::copy_option::overwrite_if_exists);

            auto image_info_src_path = new_image.parent_path() / image_info_name;
            auto image_info_dst_path = old_image.parent_path() / image_info_name;

            fs::copy_file(image_info_src_path,
                          image_info_dst_path,
                          fs::copy_option::overwrite_if_exists);
        },
        fsm.new_image_path_,
        fsm.image_path()});
    }
};

struct QemuFSM_::start_vm
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState& ts) -> void
    {
        ts.async_task_.reset(new AsyncTask{[](const fs::path vm_dir,
                                              std::shared_ptr<AtomicGuard<bp::child>> child,
                                              const cluster::option::Dispatch dispatch_options,
                                              const node::option::VMNode node_options)
        {
            bp::context ctx;
            ctx.work_directory = vm_dir.string();
            ctx.environment = bp::self::get_environment();
            ctx.stdout_behavior = bp::capture_stream();
            ctx.stderr_behavior = bp::redirect_stream_to_stdout();

            auto exe = std::string{};
            auto exe_name = std::string{};

            if(dispatch_options.vm.arch == "x86")
            {
                if(node_options.vm.path.x86.empty())
                {
                    exe = bp::find_executable_in_path("crete-qemu-2.3-system-i386");
                }
                else
                {
                    exe = node_options.vm.path.x86;
                }
            }
            else if(dispatch_options.vm.arch == "x64")
            {
                if(node_options.vm.path.x64.empty())
                {
                    exe = bp::find_executable_in_path("crete-qemu-2.3-system-x86_64");
                }
                else
                {
                    exe = node_options.vm.path.x64;
                }
            }
            else
            {
                BOOST_THROW_EXCEPTION(Exception{} << err::arg_invalid_str{dispatch_options.vm.arch}
                                                  << err::arg_invalid_str{"vm.arch"});
            }

            if(!fs::exists(exe))
            {
                BOOST_THROW_EXCEPTION(Exception{} << err::file_missing{exe});
            }

            auto args = std::vector<std::string>{fs::absolute(exe).string() // It appears our modified QEMU requires full path in argv[0]...
                                ,"-hda"
                                ,image_name
                                ,"-loadvm"
                                ,dispatch_options.vm.snapshot
                                };

            auto add_args = std::vector<std::string>{};

            boost::split(add_args
                        ,dispatch_options.vm.args
                        ,boost::is_any_of(" "));

            args.insert(args.end()
                       ,add_args.begin()
                       ,add_args.end());

            std::cerr << "qemu path: " << fs::absolute(exe).string() << std::endl;
            for(auto&& arg : args)
                std::cerr << "qemu args: " << arg << std::endl;
            std::cerr << "working dir: " << ctx.work_directory << std::endl;

            // FIXME: xxx child process can be dead-locked caused by output redirection
            child->acquire() = bp::launch(exe, args, ctx);

            sleep(1); // TODO/FIXME: xxx sleep time is subjective.

            if(!process::is_running(child->acquire()->get_id()))
            {
                BOOST_THROW_EXCEPTION(VMException{} << err::process_exited{"pid_"});
            }
        },
        fsm.vm_dir_,
        fsm.child_,
        fsm.dispatch_options_,
        fsm.node_options_});
    }
};

struct QemuFSM_::start_test
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&, TargetState&) -> void
    {
        auto hostfile = fsm.vm_dir_ / hostfile_dir_name;

        if(!fs::exists(hostfile))
        {
            fs::create_directories(hostfile);
        }

        auto in_args_path = hostfile / input_args_name;

        if(fs::exists(in_args_path))
        {
            fs::remove(in_args_path);
        }

        std::ofstream ofs{in_args_path.string().c_str()};

        if(!ofs.good())
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::file{in_args_path.string()});
        }

        write_serialized(ofs, ev.tc_);

        try
        {
            fsm.server_->write(0,
                               packet_type::cluster_next_test);
        }
        catch(std::exception& e)
        {
            BOOST_THROW_EXCEPTION(VMException{} << err::msg{boost::diagnostic_information(e)});
        }
    }
};

struct QemuFSM_::store_trace
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState& ts) -> void
    {
        ts.async_task_.reset(new AsyncTask{[](const fs::path vm_dir,
                                              std::shared_ptr<fs::path> trace)
        {
            auto trace_ready = vm_dir / hostfile_dir_name / trace_ready_name;
            auto trace_dir = vm_dir / trace_dir_name;

            CRETE_EXCEPTION_ASSERT(fs::exists(trace_ready),
                                   err::file_missing{trace_ready.string()});
            CRETE_EXCEPTION_ASSERT(fs::exists(trace_dir),
                                   err::file_missing{trace_dir.string()});
            CRETE_EXCEPTION_ASSERT(fs::remove_all(trace_dir / "runtime-dump-last") == 1,
                                   err::file_remove{(trace_dir / "runtime-dump-last").string()});

            fs::directory_iterator begin{trace_dir}, end{};
            auto pit =
                std::find_if(begin,
                             end,
                             [](const fs::path& p)
                             {
                                 return boost::starts_with(p.filename().string(), "runtime-dump");
                             });

            CRETE_EXCEPTION_ASSERT(pit != end, // Sanity check.
                                   err::file{"could not find trace to store during store-phase"});

            auto original_trace = pit->path();

            // FIXME: xxx should be redundant, as the test case can be directly written by qemu
            //            as a part of the trace
            fs::copy_file(vm_dir / hostfile_dir_name / input_args_name,
                          original_trace / "concrete_inputs.bin");

//            if(fs::exists("tb-ir.txt"))
//            {
//                fs::rename("tb-ir.txt",
//                           original_trace / "tb-ir.txt");
//            }

            auto trace_uuid = boost::uuids::random_generator{}();
            *trace = original_trace.parent_path() / boost::uuids::to_string(trace_uuid);

            fs::rename(original_trace,
                       *trace);

            fs::remove(trace_ready);

        }, fsm.vm_dir_, fsm.trace_});
    }
};

struct QemuFSM_::report_error
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM&, SourceState&, TargetState&) -> void
    {
        // TODO write error file, log test case causing the error, anything else.
    }
};

struct QemuFSM_::connect_vm
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState& ts) -> void
    {
        auto pid = fsm.child_->acquire()->get_id();

        if(!process::is_running(pid))
        {
            BOOST_THROW_EXCEPTION(VMException{} << err::process_exited{"pid_"});
        }

        ts.async_task_.reset(new AsyncTask{[](std::shared_ptr<Server> server,
                                              const fs::path vm_dir,
                                              const bool distributed,
                                              const std::string target)
        {
            auto new_port = server->port();

            std::cout << "new_port: " << new_port << std::endl;

            auto port_file_path = vm_dir / hostfile_dir_name / vm_port_file_name;

            {
                // Assumes crete-runner continuously checks until the file is created.
                fs::ofstream ofs(port_file_path,
                                 std::ios_base::out | std::ios_base::binary);

                ofs << new_port;
            }

            // TODO: should actually continously check if the VM has terminated while waiting, or have a timeout.
            server->open_connection_wait();

            std::cout << "after: fsm.server_.open_connection_wait()" << std::endl;

            // Remove the path immediately upon connection, so it isn't used erroneously in the future.
            // I.e., next time crete-run attempts to connect.
            fs::remove(port_file_path);

            try
            {
                auto pkinfo = PacketInfo{0,0,0};

                // TODO: break into logical component functions: connect, write_config, read_guest_config.
                // TODO: send configuration data. Right now, just send target.
                pkinfo.type = packet_type::cluster_config;
                write_serialized_text(*server,
                                      pkinfo,
                                      distributed);

                if(distributed)
                {
                    pkinfo.type = packet_type::cluster_next_target;
                    write_serialized_text(*server,
                                          pkinfo,
                                          target);
                }
                std::cout << "after: packet_type::cluster_next_target" << std::endl;
            }
            catch(std::exception& e)
            {
                BOOST_THROW_EXCEPTION(VMException{} << err::msg{boost::diagnostic_information(e)});
            }

            // TODO: As I don't need to send the guest config for every instance (they're all the same),
            //       do I actually need this? Only for the first instance to run.
            //       However, I do want one to receive one thing consistently: the proc-maps. I need
            //       proc-maps for a sanity check, to ensure all vm-inst proc-maps are consistent.
            //       So, what I can do is provide an optional state to enter if it's the first
            //       vm to get the the run_config info. Otherwise, always get the info.
            // TODO: Any reason to actually use RunConfiguration here? Seems I can just send the buffer on to Dispatch...
            //    config::RunConfiguration rc;
            //    boost::asio::streambuf sbuf;
            //    read_serialized_binary(*server_,
            //                           sbuf);
        },
        fsm.server_,
        fsm.vm_dir_,
        fsm.dispatch_options_.mode.distributed,
        fsm.target_});
    }
};

struct QemuFSM_::receive_guest_info
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        try
        {
            // Perform any work needed only by the first VM instance s.a. grabbing the guest config, any debug info.
            read_serialized_text_xml(*fsm.server_,
                                     fsm.guest_data_.guest_config,
                                     packet_type::guest_configuration);
        }
        catch(std::exception& e)
        {
            BOOST_THROW_EXCEPTION(VMException{} << err::msg{boost::diagnostic_information(e)});
        }
    }
};

struct QemuFSM_::finish
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM&, SourceState&, TargetState&) -> void
    {
        // TODO: anything to do here?
    }
};

struct QemuFSM_::terminate
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        auto pid = fsm.child_->acquire()->get_id();

        assert(pid);

        if(pid != -1 &&
           process::is_running(pid))
        {
            if(::kill(pid, SIGKILL) != 0)
            {
                BOOST_THROW_EXCEPTION(Exception{} << err::process{"failed to kill VM instance"}
                                                  << err::process_error{pid}
                                                  << err::c_errno{errno});
            }

            while(process::is_running(pid)) {} // TODO: is this check necessary?
        }
    }
};

// +--------------------------------------------------+
// + Guards                                           +
// +--------------------------------------------------+

struct QemuFSM_::is_prev_task_finished
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM&, SourceState& ss, TargetState&) -> bool
    {
        if(ss.async_task_->is_exception_thrown())
        {
            ss.async_task_->rethrow_exception();
        }

        return ss.async_task_->is_finished();
    }
};

struct QemuFSM_::is_first_vm
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> bool
    {
        return fsm.first_vm_;
    }
};

struct QemuFSM_::is_vm_terminated
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> bool
    {
        return !process::is_running(*fsm.pid_);
    }
};

struct QemuFSM_::is_finished
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> bool
    {
        auto pid = fsm.child_->acquire()->get_id();

        if(!process::is_running(pid)) // TODO: this covers the case where the VM died, but what about crete-run?
        {
            BOOST_THROW_EXCEPTION(VMException{} << err::process_exited{"pid_"});
        }

        auto trace_ready_sig = fsm.vm_dir_ / hostfile_dir_name / trace_ready_name;

        return fs::exists(trace_ready_sig);
    }
};

struct QemuFSM_::is_distributed
{
    template <class FSM,class SourceState,class TargetState>
    auto operator()(ev::start const& ev, FSM&, SourceState&, TargetState&) -> bool
    {
        std::cerr << "ev.dispatch_options_.mode.distributed" << std::endl;
        return ev.dispatch_options_.mode.distributed;
    }

    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> bool
    {
        std::cerr << "fsm.dispatch_options_.mode.distributed" << std::endl;
        return fsm.dispatch_options_.mode.distributed;
    }
};

struct QemuFSM_::has_next_target
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> bool
    {
        return !fsm.target_.empty();
    }
};


struct QemuFSM_::is_image_valid
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> bool
    {
        auto path = fsm.vm_dir_ / image_info_name;
        auto new_image_info_path = fsm.new_image_path_.parent_path() / image_info_name;

        if(!fs::exists(new_image_info_path))
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::file_missing{new_image_info_path.string()});
        }

        if(!fs::exists(path))
            return false;
        if(!fs::exists(fsm.vm_dir_ / image_name))
            return false;

        fs::ifstream new_ifs{new_image_info_path};

        if(!new_ifs.good())
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{new_image_info_path.string()});
        }

        fs::ifstream old_ifs{path};

        if(!old_ifs.good())
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{path.string()});
        }

        auto new_image_info = ImageInfo{};
        auto old_image_info = ImageInfo{};

        serialize::read_text(new_ifs,
                             new_image_info);
        serialize::read_text(old_ifs,
                             old_image_info);

        return old_image_info == new_image_info;
    }
};

// Normally would just: "using QemuFSM = boost::msm::back::state_machine<QemuFSM_>;",
// but needed a way to hide the impl. in source file, so as to avoid namespace troubles.
// This seems to work.
class QemuFSM : public boost::msm::back::state_machine<QemuFSM_>
{
};

}

// namespace fsm
} // namespace vm
} // namespace node
} // namespace cluster
} // namespace crete
