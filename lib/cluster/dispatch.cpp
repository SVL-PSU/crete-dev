#include <crete/cluster/dispatch.h>
#include <crete/exception.h>
#include <crete/logger.h>
#include <crete/async_task.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/range/irange.hpp>
#include <boost/msm/back/state_machine.hpp> // back-end
#include <boost/msm/front/state_machine_def.hpp> //front-end
#include <boost/msm/front/functor_row.hpp>
#include <boost/msm/front/euml/operator.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <chrono>
#include <deque>
#include <algorithm>
#include <vector>

namespace bpt = boost::property_tree;
namespace bui = boost::uuids;
namespace fs = boost::filesystem;
namespace msm = boost::msm;
namespace mpl = boost::mpl;
using namespace msm::front;
using namespace msm::front::euml;

namespace crete
{
namespace cluster
{

namespace vm
{
// +--------------------------------------------------+
// + Finite State Machine                             +
// +--------------------------------------------------+

// +--------------------------------------------------+
// + Flags                                            +
// +--------------------------------------------------+
namespace flag
{
    struct tx_config {};
    struct image {};
    struct trace_rxed {};
    struct tx_test {};
    struct error_rxed {};
    struct guest_data_rxed {};
    struct status_rxed {};
    struct active {}; // Use in the target state of an 'active' operation. TS is entered, then the action is fired.
    struct error {};
}

// +--------------------------------------------------+
// + Events                                           +
// +--------------------------------------------------+
struct start;
struct config;
struct poll {};
struct image;
struct trace {};
struct test;

// +--------------------------------------------------+
// + State Machine Front End                          +
// +--------------------------------------------------+
class VMNodeFSM_ : public boost::msm::front::state_machine_def<VMNodeFSM_>
{
public:
    VMNodeFSM_();

    auto node_status() const -> const NodeStatus&;
    auto get_trace() const -> const fs::path&;
    auto errors() const -> const std::deque<log::NodeError>&;
    auto pop_error() -> log::NodeError;
    auto guest_data() const -> const GuestData&;

    // +--------------------------------------------------+
    // + Entry & Exit                                     +
    // +--------------------------------------------------+
    template <class Event,class FSM>
    void on_entry(Event const&,FSM&)
    {
    }
    template <class Event,class FSM>
    void on_exit(Event const&,FSM&)
    {
    }

    // +--------------------------------------------------+
    // + States                                           +
    // +--------------------------------------------------+
    struct Start;
    struct TxConfig;
    struct ValidateImage;
    struct RxGuestData;
    struct GuestDataRxed;
    struct UpdateImage;
    struct Commence;
    struct RxStatus;
    struct StatusRxed;
    struct RxTrace;
    struct TxTest;
    struct TraceRxed;
    struct TestTxed;
    struct ErrorRxed;
    struct Error;

    // +--------------------------------------------------+
    // + Actions                                          +
    // +--------------------------------------------------+
    struct init;
    struct tx_config;
    struct update_image_info;
    struct update_image;
    struct rx_guest_data;
    struct commence;
    struct rx_status;
    struct rx_trace;
    struct tx_test;
    struct rx_error;

    // +--------------------------------------------------+
    // + Gaurds                                           +
    // +--------------------------------------------------+
    struct is_prev_task_finished;
    struct is_distributed;
    struct do_update;
    struct is_first_vm_node;
    struct is_image_valid;
    struct has_trace;
    struct has_error;

    // +--------------------------------------------------+
    // + Transitions                                      +
    // +--------------------------------------------------+

    template <class FSM,class Event>
    void exception_caught(Event const&,FSM& fsm,std::exception& e)
    {
        (void)fsm;
        // TODO: transition to error state.
//        fsm.process_event(ErrorConnection());
        std::cerr << boost::diagnostic_information(e) << std::endl;
        assert(0 && "VMNodeFSM_: exception thrown from within FSM");
    }

    // Initial state of the FSM.
    using initial_state = Start;

    struct transition_table : mpl::vector
    <
    //    Start              Event              Target             Action                Guard
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Start             ,start             ,TxConfig          ,init                 ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<TxConfig          ,config            ,ValidateImage     ,tx_config            ,And_<is_distributed,
                                                                                              do_update>      >,
      Row<TxConfig          ,config            ,Commence          ,tx_config            ,And_<is_distributed,
                                                                                              Not_<do_update>> >,
      Row<TxConfig          ,config            ,Commence          ,tx_config            ,Not_<is_distributed> >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<ValidateImage     ,image             ,UpdateImage       ,none                 ,Not_<is_image_valid> >,
      Row<ValidateImage     ,image             ,Commence          ,none                 ,is_image_valid       >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<UpdateImage       ,image             ,Commence          ,ActionSequence_<mpl::vector<
                                                                       update_image,
                                                                       update_image_info>> ,none              >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Commence          ,poll              ,RxGuestData       ,commence             ,And_<is_prev_task_finished,
                                                                                            is_first_vm_node> >,
      Row<Commence          ,poll              ,RxStatus          ,commence             ,And_<is_prev_task_finished,
                                                                                          Not_<is_first_vm_node>> >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<RxGuestData       ,poll              ,GuestDataRxed     ,rx_guest_data        ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<GuestDataRxed     ,poll              ,RxStatus          ,none                 ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<RxStatus          ,poll              ,StatusRxed        ,rx_status            ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<StatusRxed        ,poll              ,RxTrace           ,none                 ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<RxTrace           ,poll              ,TxTest            ,none                 ,Not_<has_trace>      >,
      Row<RxTrace           ,poll              ,TraceRxed         ,rx_trace             ,has_trace            >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<TraceRxed         ,trace             ,TxTest            ,none                 ,is_prev_task_finished>,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<TxTest            ,test              ,TestTxed          ,ActionSequence_<mpl::vector<
                                                                       tx_test,
                                                                       rx_status>>      ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<TestTxed          ,poll              ,RxStatus          ,none                 ,Not_<has_error>      >,
      Row<TestTxed          ,poll              ,ErrorRxed         ,rx_error             ,has_error            >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<ErrorRxed         ,poll              ,RxStatus          ,none                 ,none                 >
    > {};

private:
    NodeRegistrar::Node node_;
    bool first_vm_node_{false};
    fs::path traces_dir_;
    std::shared_ptr<fs::path> trace_ = std::make_shared<fs::path>();
    std::deque<log::NodeError> errors_;
    boost::optional<ImageInfo> image_info_;
    bool update_image_{false};
    bool distributed_{false};
    GuestData guest_data_;
};

struct start
{
    start(const NodeRegistrar::Node& node,
          bool first,
          const fs::path& traces_dir,
          bool update_image,
          bool distributed) :
        node_{node},
        first_vm_node_{first},
        traces_dir_{traces_dir},
        update_image_{update_image},
        distributed_{distributed}
    {
    }

    NodeRegistrar::Node node_;
    bool first_vm_node_{false};
    fs::path traces_dir_;
    bool update_image_{false};
    bool distributed_{false};
};

struct config
{
    config(const option::Dispatch& options) :
        options_(options)
    {}

    option::Dispatch options_;
};

struct image
{
    image(const fs::path& image_path) :
        image_path_(image_path)
    {}

    fs::path image_path_;
};

struct test
{
    std::vector<TestCase> tests_;
};

VMNodeFSM_::VMNodeFSM_()
{
}

//auto VMNodeFSM_::node() -> NodeRegistrar::Node&
//{
//    return node_;
//}

auto VMNodeFSM_::node_status() const -> const NodeStatus&
{
    return node_->acquire()->status;
}

auto VMNodeFSM_::get_trace() const -> const fs::path&
{
    return *trace_;
}

auto VMNodeFSM_::errors() const -> const std::deque<log::NodeError>&
{
    return errors_;
}

auto VMNodeFSM_::pop_error() -> log::NodeError
{
    auto e = errors_.front();

    errors_.pop_front();

    return e;
}

auto VMNodeFSM_::guest_data() const -> const GuestData&
{
    return guest_data_;
}

// +--------------------------------------------------+
// + States                                           +
// +--------------------------------------------------+

struct VMNodeFSM_::Start : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Start" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Start" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::TxConfig : public msm::front::state<>
{
    using flag_list = mpl::vector2<flag::tx_config, flag::active>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: TxConfig" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: TxConfig" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::ValidateImage : public msm::front::state<>
{
    using flag_list = mpl::vector2<flag::image, flag::active>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: ValidateImage" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: ValidateImage" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::RxGuestData : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::active>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: RxGuestData" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: RxGuestData" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::GuestDataRxed : public msm::front::state<>
{
    using flag_list = mpl::vector2<flag::guest_data_rxed, flag::active>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: GuestDataRxed" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: GuestDataRxed" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::UpdateImage : public msm::front::state<>
{
    using flag_list = mpl::vector2<flag::image, flag::active>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: UpdateImage" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: UpdateImage" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::Commence : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::active>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Commence" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Commence" << std::endl;}
#endif // defined(CRETE_DEBUG)

    std::unique_ptr<AsyncTask> async_task_{new AsyncTask{}};
};

struct VMNodeFSM_::RxStatus : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: RxStatus" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: RxStatus" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::StatusRxed : public msm::front::state<>
{
    using flag_list = mpl::vector2<flag::status_rxed, flag::active>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: StatusRxed" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: StatusRxed" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::RxTrace : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: RxTrace" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: RxTrace" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::TxTest : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::tx_test>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: TxTest" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: TxTest" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::TraceRxed : public msm::front::state<>
{
    using flag_list = mpl::vector2<flag::trace_rxed, flag::active>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: TraceRxed" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: TraceRxed" << std::endl;}
#endif // defined(CRETE_DEBUG)

    std::unique_ptr<AsyncTask> async_task_;
};

struct VMNodeFSM_::TestTxed : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::active>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: TestTxed" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: TestTxed" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::ErrorRxed : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::error_rxed>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: ErrorRxed" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: ErrorRxed" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::Error : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::error>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Error" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Error" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

// +--------------------------------------------------+
// + Actions                                          +
// +--------------------------------------------------+

struct VMNodeFSM_::init
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&, TargetState&) -> void
    {
        fsm.node_ = ev.node_;
        fsm.first_vm_node_ = ev.first_vm_node_;
        fsm.update_image_ = ev.update_image_;
        fsm.distributed_ = ev.distributed_;
        fsm.traces_dir_ = ev.traces_dir_;
    }
};

struct VMNodeFSM_::tx_config
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&, TargetState&) -> void
    {
        transmit_config(fsm.node_,
                        ev.options_);
    }
};

struct VMNodeFSM_::update_image_info
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&, TargetState&) -> void
    {
        transmit_image_info(fsm.node_,
                            ImageInfo{ev.image_path_});
    }
};

struct VMNodeFSM_::update_image
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&, TargetState& ts) -> void
    {
        ts.async_task_.reset(new AsyncTask{[]( NodeRegistrar::Node node
                                             , const fs::path image_path)
        {
            if(!fs::exists(image_path))
            {
                BOOST_THROW_EXCEPTION(Exception{} << err::file_missing{image_path.string()});
            }

            fs::ifstream ifs(image_path);

            CRETE_EXCEPTION_ASSERT(ifs.good(), err::file_open_failed{image_path.string()});

            auto pkinfo = PacketInfo{0,0,0};
            auto lock = node->acquire();

            pkinfo.id = lock->status.id;
            pkinfo.type = packet_type::cluster_image;

            lock->server.write(pkinfo);

            std::cout << "Sending OS image to VM Node..." << std::endl;

            write(lock->server,
                  ifs,
                  default_chunk_size);

        }
        , fsm.node_
        , ev.image_path_});
    }
};

struct VMNodeFSM_::rx_guest_data
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        auto lock = fsm.node_->acquire();

        auto pkinfo = PacketInfo{0,0,0};
        pkinfo.id = lock->status.id;
        pkinfo.type = packet_type::cluster_request_guest_data;

        lock->server.write(pkinfo);

        read_serialized_binary(lock->server,
                               fsm.guest_data_,
                               packet_type::cluster_tx_guest_data);
    }
};

struct VMNodeFSM_::commence
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        transmit_commencement(fsm.node_);
    }
};

struct VMNodeFSM_::rx_status
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        cluster::poll(fsm.node_);
    }
};

struct VMNodeFSM_::rx_trace
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState& ts) -> void
    {
        ts.async_task_.reset(new AsyncTask{[]( NodeRegistrar::Node node
                                             , const fs::path traces_dir
                                             , std::shared_ptr<fs::path> trace)
        {
            *trace = receive_trace(node,
                                  traces_dir);
        }
        , fsm.node_
        , fsm.traces_dir_
        , fsm.trace_});
    }
};

struct VMNodeFSM_::tx_test
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&, TargetState&) -> void
    {
        if(ev.tests_.empty())
        {
            return;
        }

        transmit_tests(fsm.node_,
                       ev.tests_);
    }
};

struct VMNodeFSM_::rx_error
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        auto v = receive_errors(fsm.node_);

        fsm.errors_ = std::deque<log::NodeError>{v.begin(),
                                                 v.end()};
    }
};

// +--------------------------------------------------+
// + Gaurds                                           +
// +--------------------------------------------------+

struct VMNodeFSM_::is_prev_task_finished // TODO: this struct should be a general utility available to all FSMs, not belonging to any FSM struct.
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

struct VMNodeFSM_::is_distributed
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> bool
    {
        return fsm.distributed_;
    }
};

struct VMNodeFSM_::do_update
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> bool
    {
        return fsm.update_image_;
    }
};

struct VMNodeFSM_::is_first_vm_node
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&,TargetState&) -> bool
    {
        return fsm.first_vm_node_;
    }
};

struct VMNodeFSM_::is_image_valid
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&,TargetState&) -> bool
    {
        if(!fsm.image_info_)
        {
            fsm.image_info_ = receive_image_info(fsm.node_);
        }

        auto dispatch_image_info = ImageInfo{ev.image_path_};

        // Node has no image.
        if(fsm.image_info_->file_name_.empty())
        {
            return false;
        }

        // Node image needs upating
        if(dispatch_image_info != *fsm.image_info_)
        {
            return false;
        }

        return true;
    }
};

struct VMNodeFSM_::has_trace
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&,TargetState&) -> bool
    {
        return fsm.node_->acquire()->status.trace_count > 0;
    }
};

struct VMNodeFSM_::has_error
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&,TargetState&) -> bool
    {
        return fsm.node_->acquire()->status.error_count > 0;
    }
};

// Normally would just: "using NodeFSM = boost::msm::back::state_machine<VMNodeFSM_>;",
// but needed a way to hide the impl. in source file, so as to avoid namespace troubles.
// This seems to work.
class NodeFSM : public boost::msm::back::state_machine<VMNodeFSM_>
{
};

} // namespace vm

namespace svm
{
// +--------------------------------------------------+
// + Finite State Machine                             +
// +--------------------------------------------------+

// +--------------------------------------------------+
// + Flags                                            +
// +--------------------------------------------------+
namespace flag
{
    using status_rxed = vm::flag::status_rxed;
    struct test_rxed {};
    struct tx_trace {};
    using active = vm::flag::active;
    struct error {};
}

// +--------------------------------------------------+
// + Events                                           +
// +--------------------------------------------------+
struct start;
using config = vm::config;
struct poll {};
struct trace;
struct test {};

// +--------------------------------------------------+
// + State Machine Front End                          +
// +--------------------------------------------------+
class SVMNodeFSM_ : public boost::msm::front::state_machine_def<SVMNodeFSM_>
{
private:
    NodeRegistrar::Node node_;
    std::vector<TestCase> tests_;
    std::deque<log::NodeError> errors_;

    friend class vm::VMNodeFSM_; // Allow reuse of VMNode's actions/guards with private members.

public:
    SVMNodeFSM_();

    auto node_status() -> const NodeStatus&;
    auto tests() -> const std::vector<TestCase>&;
    auto errors() -> const std::deque<log::NodeError>&;
    auto pop_error() -> const log::NodeError;

    // +--------------------------------------------------+
    // + Entry & Exit                                     +
    // +--------------------------------------------------+
    template <class Event,class FSM>
    void on_entry(Event const&,FSM&)
    {
    }
    template <class Event,class FSM>
    void on_exit(Event const&,FSM&)
    {
    }

    // +--------------------------------------------------+
    // + States                                           +
    // +--------------------------------------------------+
    struct Start;
    using TxConfig = vm::NodeFSM::TxConfig;
    struct Commence;
    using  RxStatus = vm::NodeFSM::RxStatus;
    using  StatusRxed = vm::NodeFSM::StatusRxed;
    struct TxTrace;
    struct TraceTxed;
    struct RxTest;
    struct TestRxed;
    using ErrorRxed = vm::NodeFSM::ErrorRxed;
    struct Error;

    // +--------------------------------------------------+
    // + Actions                                          +
    // +--------------------------------------------------+
    struct init;
    using tx_config = vm::NodeFSM::tx_config;
    struct commence;
    struct tx_trace;
    struct rx_test;
    using rx_status = vm::NodeFSM::rx_status;
    using rx_error = vm::NodeFSM::rx_error;

    // +--------------------------------------------------+
    // + Gaurds                                           +
    // +--------------------------------------------------+
    using is_prev_task_finished = vm::NodeFSM::is_prev_task_finished;
    struct has_tests;
    using has_error = vm::NodeFSM::has_error;

    // +--------------------------------------------------+
    // + Transitions                                      +
    // +--------------------------------------------------+

    template <class FSM,class Event>
    void exception_caught(Event const&,FSM& fsm,std::exception& e)
    {
        (void)fsm;
        // TODO: transition to error state.
//        fsm.process_event(ErrorConnection());
        std::cerr << boost::diagnostic_information(e) << std::endl;
        assert(0 && "VMNodeFSM_: exception thrown from within FSM");
    }

    // Initial state of the FSM.
    using initial_state = Start;

    struct transition_table : mpl::vector
    <
    //    Start              Event              Target             Action                Guard
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Start             ,start             ,TxConfig          ,init                 ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<TxConfig          ,config            ,Commence          ,tx_config            ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Commence          ,poll              ,RxStatus          ,commence             ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<RxStatus          ,poll              ,StatusRxed        ,rx_status            ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<StatusRxed        ,poll              ,TxTrace           ,none                 ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<TxTrace           ,trace             ,TraceTxed         ,tx_trace             ,none                 >,
      Row<TxTrace           ,poll              ,RxTest            ,none                 ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<TraceTxed         ,poll              ,RxTest            ,rx_status            ,is_prev_task_finished>,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<RxTest            ,poll              ,RxStatus          ,none                 ,And_<Not_<has_tests>,
                                                                                              Not_<has_error>>>,
      Row<RxTest            ,poll              ,ErrorRxed         ,rx_error             ,And_<Not_<has_tests>,
                                                                                              has_error>      >,
      Row<RxTest            ,poll              ,TestRxed          ,rx_test              ,has_tests            >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<TestRxed          ,test              ,RxStatus          ,none                 ,Not_<has_error>      >,
      Row<TestRxed          ,test              ,ErrorRxed         ,rx_error             ,has_error            >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<ErrorRxed         ,poll              ,RxStatus          ,none                 ,none                 >
    > {};
};

struct start
{
    start(NodeRegistrar::Node& node) :
        node_{node}
    {}

    NodeRegistrar::Node node_;
};

struct trace
{
    fs::path trace_;
};

SVMNodeFSM_::SVMNodeFSM_()
{
}

auto SVMNodeFSM_::node_status() -> const NodeStatus&
{
    return node_->acquire()->status;
}

auto SVMNodeFSM_::tests() -> const std::vector<TestCase>&
{
    return tests_;
}

auto SVMNodeFSM_::errors() -> const std::deque<log::NodeError>&
{
    return errors_;
}

auto SVMNodeFSM_::pop_error() -> const log::NodeError
{
    auto e = errors_.front();

    errors_.pop_front();

    return e;
}

// +--------------------------------------------------+
// + States                                           +
// +--------------------------------------------------+

struct SVMNodeFSM_::Start : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Start" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Start" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct SVMNodeFSM_::Commence : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::active>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Commence" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Commence" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct SVMNodeFSM_::TxTrace : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::tx_trace>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: TxTrace" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: TxTrace" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct SVMNodeFSM_::TraceTxed : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::active>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: TraceTxed" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: TraceTxed" << std::endl;}
#endif // defined(CRETE_DEBUG)

    std::unique_ptr<AsyncTask> async_task_;
};

struct SVMNodeFSM_::RxTest : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: RxTest" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: RxTest" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct SVMNodeFSM_::TestRxed : public msm::front::state<>
{
    using flag_list = mpl::vector2<flag::test_rxed, flag::active>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: TestRxed" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: TestRxed" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct SVMNodeFSM_::Error : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::error>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Error" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Error" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

// +--------------------------------------------------+
// + Actions                                          +
// +--------------------------------------------------+

struct SVMNodeFSM_::init
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&, TargetState&) -> void
    {
        fsm.node_ = ev.node_;
    }
};

struct SVMNodeFSM_::commence
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        transmit_commencement(fsm.node_);
    }
};

struct SVMNodeFSM_::tx_trace
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&, TargetState& ts) -> void
    {
        ts.async_task_.reset(new AsyncTask{[]( NodeRegistrar::Node node
                                             , const fs::path trace)
        {
            transmit_trace(node,
                           trace);
        }
        , fsm.node_
        , ev.trace_});

    }
};

struct SVMNodeFSM_::rx_test
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        fsm.tests_ = receive_tests(fsm.node_);
    }
};

// +--------------------------------------------------+
// + Gaurds                                           +
// +--------------------------------------------------+

struct SVMNodeFSM_::has_tests
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> bool
    {
        return fsm.node_->acquire()->status.test_case_count > 0;
    }
};

// Normally would just: "using NodeFSM = boost::msm::back::state_machine<SVMNodeFSM_>;",
// but needed a way to hide the impl. in source file, so as to avoid namespace troubles.
// This seems to work.
class NodeFSM : public boost::msm::back::state_machine<SVMNodeFSM_>
{
};

} // namespace svm

namespace fsm
{

// +--------------------------------------------------+
// + Finite State Machine                             +
// +--------------------------------------------------+

// +--------------------------------------------------+
// + Flags                                            +
// +--------------------------------------------------+
namespace flag
{
struct terminated {};
}

// +--------------------------------------------------+
// + Events                                           +
// +--------------------------------------------------+
struct start;
struct poll {};

// +--------------------------------------------------+
// + State Machine Front End                          +
// +--------------------------------------------------+
class DispatchFSM_ : public boost::msm::front::state_machine_def<DispatchFSM_>
{
public:
    using VMNodeFSM = std::shared_ptr<vm::NodeFSM>;
    using VMNodeFSMs = std::vector<VMNodeFSM>;
    using SVMNodeFSM = std::shared_ptr<svm::NodeFSM>;
    using SVMNodeFSMs = std::vector<SVMNodeFSM>;

public:
    DispatchFSM_();
    ~DispatchFSM_();

    auto to_trace_pool(const fs::path& trace) -> void;
    auto next_trace() -> boost::optional<fs::path>;
    auto next_test() -> boost::optional<TestCase>;
    auto node_registrar() -> AtomicGuard<NodeRegistrar>&;
    auto display_status(std::ostream& os) -> void;
    auto write_tc_tree(std::ostream& os) -> void;
    auto write_statistics() -> void;
    auto test_pool() -> TestPool&;
    auto trace_pool() -> TracePool&;
    auto set_up_root_dir() -> void;
    auto launch_node_registrar(Port master) -> void;
    auto elapsed_time() -> uint64_t;
    auto are_node_queues_empty() -> bool;
    auto are_all_queues_empty() -> bool;
    auto are_nodes_inactive() -> bool;
    auto is_converged() -> bool;
    auto write_target_log(const log::NodeError& ne,
                          const fs::path& subdir) -> void;

    // +--------------------------------------------------+
    // + Entry & Exit                                     +
    // +--------------------------------------------------+
    template <class Event,class FSM>
    void on_entry(Event const&,FSM&)
    {
    }
    template <class Event,class FSM>
    void on_exit(Event const&,FSM&)
    {
    }

    // +--------------------------------------------------+
    // + States                                           +
    // +--------------------------------------------------+
    struct Start;
    struct SpecCheck;
    struct NextTarget;
    struct Dispatch;
    struct Terminate;
    struct Terminated;

    // +--------------------------------------------------+
    // + Actions                                          +
    // +--------------------------------------------------+
    struct init;
    struct reset;
    struct assign_next_target;
    struct dispatch;
    struct finish;
    struct next_target_clean;
    struct terminate;

    // +--------------------------------------------------+
    // + Gaurds                                           +
    // +--------------------------------------------------+
    struct is_first;
    struct is_dev_mode;
    struct have_next_target;
    struct is_target_expired;

    // +--------------------------------------------------+
    // + Transitions                                      +
    // +--------------------------------------------------+

    template <class FSM,class Event>
    void exception_caught(Event const&,FSM& fsm,std::exception& e)
    {
        (void)fsm;
        // TODO: transition to error state.
//        fsm.process_event(ErrorConnection());
        std::cerr << boost::diagnostic_information(e) << std::endl;
        assert(0 && "VMNodeFSM_: exception thrown from within FSM");
    }

    // Initial state of the FSM.
    using initial_state = Start;

    struct transition_table : mpl::vector
    <
    //    Start              Event              Target             Action                Guard
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Start             ,start             ,SpecCheck         ,init                 ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<SpecCheck         ,poll              ,NextTarget        ,none                 ,And_<Not_<is_dev_mode>,
                                                                                              Or_<is_first,
                                                                                                  And_<is_target_expired,
                                                                                                       have_next_target>>>>,
      Row<SpecCheck         ,poll              ,Dispatch          ,none                 ,Or_<is_dev_mode,
                                                                                             And_<Not_<is_first>,
                                                                                                  Not_<is_target_expired>>>>,
      Row<SpecCheck         ,poll              ,Terminate         ,next_target_clean    ,And_<is_target_expired,
                                                                                              Not_<have_next_target>>>,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<NextTarget        ,poll              ,Dispatch          ,ActionSequence_<mpl::vector<
                                                                       finish,
                                                                       next_target_clean,
                                                                       reset,
                                                                       assign_next_target>>
                                                                                        ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Dispatch          ,poll              ,SpecCheck         ,dispatch             ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Terminate         ,poll              ,Terminated        ,ActionSequence_<mpl::vector<
                                                                       finish,
                                                                       terminate>>      ,none                 >
    > {};

private:
    option::Dispatch options_;
    AtomicGuard<NodeRegistrar> node_registrar_;
    boost::thread node_registrar_driver_thread_;
    boost::filesystem::path root_{make_dispatch_root()};
    TestPool test_pool_{root_};
//    TracePool trace_pool_{option::Dispatch{}, "weighted"};
    TracePool trace_pool_{option::Dispatch{}};
    AtomicGuard<VMNodeFSMs> vm_node_fsms_;
    AtomicGuard<SVMNodeFSMs> svm_node_fsms_;
    Port master_port_;
    crete::log::Logger exception_log_;
    crete::log::Logger node_error_log_;

    std::chrono::time_point<std::chrono::system_clock> start_time_ = std::chrono::system_clock::now();
    bool first_{true};
    std::deque<std::string> next_target_queue_;
    std::deque<std::string> next_target_seeds_queue_;
    std::string target_;
    boost::filesystem::path current_target_seeds_;
    bool first_trace_rxed_{false};
    GuestData guest_data_;
};

struct start
{
    start(Port master_port,
          const option::Dispatch& options)
        : options_{options}
        , master_port_{master_port} {}

    const option::Dispatch& options_;
    Port master_port_;
};

// +--------------------------------------------------+
// + States                                           +
// +--------------------------------------------------+

struct DispatchFSM_::Start : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const&,FSM&) {std::cout << "entering: Start" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Start" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct DispatchFSM_::SpecCheck : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const&,FSM&) {std::cout << "entering: SpecCheck" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: SpecCheck" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct DispatchFSM_::NextTarget : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const&,FSM&) {std::cout << "entering: NextTarget" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: NextTarget" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct DispatchFSM_::Dispatch : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const&,FSM&) {std::cout << "entering: Dispatch" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Dispatch" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct DispatchFSM_::Terminate : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const&,FSM&) {std::cout << "entering: Terminate" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Terminate" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct DispatchFSM_::Terminated : public msm::front::terminate_state<>
{
    using flag_list = mpl::vector1<flag::terminated>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const&,FSM&) {std::cout << "entering: Terminated" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Terminated" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

// +--------------------------------------------------+
// + Gaurds                                           +
// +--------------------------------------------------+

struct DispatchFSM_::is_first
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> bool
    {
        return fsm.first_;
    }
};

struct DispatchFSM_::is_dev_mode
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> bool
    {
        return !fsm.options_.mode.distributed;
    }
};

struct DispatchFSM_::have_next_target
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> bool
    {
        return !fsm.next_target_queue_.empty();
    }
};

struct DispatchFSM_::is_target_expired
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> bool
    {
        auto elapsed_time_count = fsm.elapsed_time();

        assert(elapsed_time_count >= 0);

        auto converged = fsm.is_converged();
        auto trace_exceeded = fsm.trace_pool_.count_all_unique() >= fsm.options_.test.interval.trace;
        auto tc_exceeded = fsm.test_pool_.count_all() >= fsm.options_.test.interval.tc;
        auto time_exceeded = elapsed_time_count >= fsm.options_.test.interval.time;

        return
                   converged
                || trace_exceeded
                || tc_exceeded
                || time_exceeded
                ;
    }
};

// +--------------------------------------------------+
// + Actions                                          +
// +--------------------------------------------------+

struct DispatchFSM_::init
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&, TargetState&) -> void
    {
        fsm.exception_log_.add_sink(fsm.root_ / log_dir_name / exception_log_file_name);
        fsm.exception_log_.auto_flush(true);
        fsm.node_error_log_.add_sink(fsm.root_ / log_dir_name / dispatch_node_error_log_file_name);
        fsm.node_error_log_.auto_flush(true);

        fsm.master_port_ = ev.master_port_;
        fsm.options_ = ev.options_;
        fsm.next_target_queue_ = std::deque<std::string>{fsm.options_.test.items.begin(),
                                                         fsm.options_.test.items.end()};
        if(!fsm.options_.test.seeds.empty()) {
            fsm.next_target_seeds_queue_ = std::deque<std::string>{fsm.options_.test.seeds.begin(),
                fsm.options_.test.seeds.end()};

            assert(fsm.next_target_seeds_queue_.size() == fsm.next_target_queue_.size()); // FIXME: xxx use exception
        }

        fsm.trace_pool_ = TracePool{fsm.options_};

        fsm.launch_node_registrar(fsm.master_port_);

        if(!fsm.options_.mode.distributed) // TODO: should be encoded into FSM.
        {
            fsm.set_up_root_dir();
        }
    }
};

struct DispatchFSM_::reset
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        fsm.set_up_root_dir();

        fsm.test_pool_ = TestPool{fsm.root_};
        fsm.trace_pool_ = TracePool{fsm.options_};

        fsm.vm_node_fsms_.acquire()->clear();
        fsm.svm_node_fsms_.acquire()->clear();

        fsm.start_time_ = std::chrono::system_clock::now();
        fsm.first_trace_rxed_ = false;

        {
            auto lock = fsm.node_registrar_.acquire();

            for(auto& node : lock->nodes())
            {
                {
                    auto nl = node->acquire();
                    auto pkinfo = PacketInfo{0,0,0};
                    pkinfo.id = nl->status.id;
                    pkinfo.type = packet_type::cluster_reset;

                    nl->server.write(pkinfo);
                }

                register_node_fsm(node,
                                  fsm.options_,
                                  fsm.root_,
                                  fsm.vm_node_fsms_,
                                  fsm.svm_node_fsms_);
            }

        }
    }
};

struct DispatchFSM_::assign_next_target
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        {
            auto lock = fsm.node_registrar_.acquire();

            for(auto& node : lock->nodes())
            {
                auto nl = node->acquire();

                if(nl->type != packet_type::cluster_request_vm_node)
                {
                    continue;
                }

                auto pkinfo = PacketInfo{0,0,0};
                pkinfo.id = nl->status.id;
                pkinfo.type = packet_type::cluster_next_target;

                auto& queue = fsm.next_target_queue_;
                write_serialized_binary(nl->server,
                                        pkinfo,
                                        queue.front());
                fsm.target_ = queue.front();
                queue.pop_front();
            }
        }
    }
};

struct DispatchFSM_::dispatch
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        {
            auto vmns_lock = fsm.vm_node_fsms_.acquire();

            std::for_each(vmns_lock->begin(),
                          vmns_lock->end(),
                          [&] (VMNodeFSM& nfsm)
            {
                if(nfsm->is_flag_active<vm::flag::trace_rxed>())
                {
                    using boost::msm::back::HANDLED_TRUE;

                    if(!fsm.first_trace_rxed_)
                    {
                        fsm.first_trace_rxed_ = true;
                    }

                    if(HANDLED_TRUE == nfsm->process_event(vm::trace{}))
                    {
                        fsm.to_trace_pool(nfsm->get_trace());
                    }
                }
                else if(nfsm->is_flag_active<vm::flag::tx_test>())
                {
                    auto tests = std::vector<TestCase>{};
                    auto tc_count = nfsm->node_status().test_case_count;

                    while(tc_count < (1*vm_test_multiplier)) // TODO: should be num_vm_insts*vm_test_multiplier. Also, should verify bandwidth, though I doubt this would be a problem.
                    {
                        auto next = fsm.next_test();

                        if(!next)
                            break;

                        tests.emplace_back(*next);

                        ++tc_count;
                    }

                    nfsm->process_event(vm::test{tests});
                }
                else if(nfsm->is_flag_active<vm::flag::error_rxed>())
                {
                    while(!nfsm->errors().empty())
                    {
                        auto err = nfsm->pop_error();

                        fsm.write_target_log(err, dispatch_log_vm_dir_name);
                        fsm.node_error_log_ << "Target: " << fsm.target_ << "\n"
                                            <<  err.log << "\n";
                    }

                    nfsm->process_event(vm::poll{});
                }
                else if(nfsm->is_flag_active<vm::flag::tx_config>())
                {
                    nfsm->process_event(vm::config{fsm.options_});
                }
                else if(nfsm->is_flag_active<vm::flag::image>())
                {
                    nfsm->process_event(vm::image{fsm.options_.vm.image.path});
                }
                else if(nfsm->is_flag_active<vm::flag::guest_data_rxed>())
                {
                    fsm.guest_data_ = nfsm->guest_data();
                    fsm.guest_data_.write_guest_config(fsm.root_ / dispatch_guest_data_dir_name / dispatch_guest_config_file_name);

                    if(fsm.options_.test.seeds.empty())
                    {
                        if(fsm.options_.vm.initial_tc.get_elements().size() > 0)
                        {
                            fsm.test_pool_.insert(fsm.options_.vm.initial_tc);
                        }
                        else
                        {
                            fsm.test_pool_.insert_initial_tc_from_config(extract_initial_test(fsm.guest_data_.guest_config));
                        }
                    }
                    else
                    {
                        fsm.current_target_seeds_ = fsm.next_target_seeds_queue_.front();
                        fsm.next_target_seeds_queue_.pop_front();
                        assert(fsm.next_target_seeds_queue_.size() == fsm.next_target_queue_.size()); // FIXME: xxx use exception

                        std::vector<TestCase> seeds = retrieve_tests(fsm.current_target_seeds_.string());
                        fsm.test_pool_.insert(seeds);
                    }

                    nfsm->process_event(vm::poll{});
                }
                else
                {
                    nfsm->process_event(vm::poll{});
                }
            });
        }

        {
            auto svmns_lock = fsm.svm_node_fsms_.acquire();

            std::for_each(svmns_lock->begin(),
                          svmns_lock->end(),
                          [&] (SVMNodeFSM& nfsm)
            {
                if(nfsm->is_flag_active<svm::flag::test_rxed>())
                {
                    // Assumption from svm_node The last test case is the input tc
                    std::vector<TestCase> new_tcs = nfsm->tests();
                    TestCase input_tc = new_tcs.back();
                    new_tcs.pop_back();
                    fsm.test_pool_.insert(new_tcs, input_tc);

                    nfsm->process_event(svm::test{});
                }
                else if(nfsm->is_flag_active<svm::flag::tx_trace>())
                {
                    auto trace_count = nfsm->node_status().trace_count;
                    auto next = boost::optional<fs::path>{};

                    if(trace_count < (1*vm_trace_multiplier)) // TODO: should be num_vm_insts*vm_trace_multiplier. Also, should verify bandwidth, though I doubt this would be a problem.
                    {

                        try // TODO: I don't like using try/catch here, but the trace could fail somehow (bug) and we need to continue testing. Have a better way?
                        {   // Cont: what seems to be causing the bug is that a supergraph is found which in turn causes a callback to call and remove it from the trace pool.
                            // Cont: This, then, seems to cause the problem. Some reference to the trace is removed, while another is perserved. When lookup is done based on the preserved, the removed raises an exception.
                            next = fsm.next_trace();
                        }
                        catch(std::exception& e)
                        {
                            fsm.exception_log_
                                    << boost::diagnostic_information(e)
                                    << "\n"
                                    << __FILE__
                                    << ": "
                                    << __LINE__
                                    << "\n";
                        }

                    }

                    if(next)
                    {
                        nfsm->process_event(svm::trace{*next});
                    }
                    else
                    {
                        nfsm->process_event(svm::poll{});
                    }
                }
                else if(nfsm->is_flag_active<vm::flag::error_rxed>())
                {
                    while(!nfsm->errors().empty())
                    {
                        auto err = nfsm->pop_error();

                        fsm.write_target_log(err, dispatch_log_svm_dir_name);
                        fsm.node_error_log_ << "Target: " << fsm.target_ << "\n"
                                            <<  err.log << "\n";
                    }

                    nfsm->process_event(svm::poll{});
                }
                else if(nfsm->is_flag_active<vm::flag::tx_config>())
                {
                    nfsm->process_event(vm::config{fsm.options_});
                }
                else
                {
                    nfsm->process_event(svm::poll{});
                }
            });
        }

        fsm.first_ = false;

        fsm.display_status(std::cout);
        fsm.write_statistics();
    }
};

struct DispatchFSM_::next_target_clean
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        // No need to store expensive traces once we're done testing.
        fs::remove_all(fsm.root_ / dispatch_trace_dir_name);
    }
};

struct DispatchFSM_::terminate
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        // TODO: Hacky means of shutting down NodeRegistrar.
        // Cont: we shouldn't have to pretend to be a node to cause it to shut down.
        Client client{"localhost", std::to_string(fsm.master_port_)};

        client.connect();

        client.write(PacketInfo{0, 0, packet_type::cluster_shutdown});
    }
};

struct DispatchFSM_::finish
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        auto p = fsm.root_ / log_dir_name;

        if(!fs::exists(p)) // TODO: encode as part of transition table. We only want to write the 'finish' file for existing tests, not the first time NextTest entered.
        {
            return;
        }

        auto finish_log = p / dispatch_log_finish_file_name;

        fs::ofstream ofs{finish_log};

        if(!ofs.good())
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{finish_log.string()});
        }

        fsm.display_status(ofs);

        auto test_case_tree_log = p / dispatch_log_test_case_tree_file_name;
        fs::ofstream tc_tree_ofs{test_case_tree_log};

        if(!tc_tree_ofs.good())
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{test_case_tree_log.string()});
        }

        fsm.write_tc_tree(tc_tree_ofs);
    }
};

DispatchFSM_::DispatchFSM_()
{
}

DispatchFSM_::~DispatchFSM_()
{
    if(node_registrar_driver_thread_.joinable())
    {
        node_registrar_driver_thread_.join();
    }
}

auto DispatchFSM_::to_trace_pool(const fs::path& trace) -> void
{
    CRETE_EXCEPTION_ASSERT(fs::exists(trace), err::file_missing{trace.string()})

    trace_pool_.insert(trace);
}

auto DispatchFSM_::next_trace() -> boost::optional<fs::path>
{
    return trace_pool_.next();
}

auto DispatchFSM_::next_test() -> boost::optional<TestCase>
{
    return test_pool_.next();
}

auto DispatchFSM_::launch_node_registrar(Port master) -> void
{
    node_registrar_driver_thread_ =
            boost::thread{NodeRegistrarDriver{master,
                          node_registrar_,
                          [this] (NodeRegistrar::Node& node) {
        register_node_fsm(node,
                          options_,
                          root_,
                          vm_node_fsms_,
                          svm_node_fsms_);
    }}};
}

auto DispatchFSM_::elapsed_time() -> uint64_t
{
    using namespace std::chrono;

    auto current_time = system_clock::now();

    return duration_cast<seconds>(current_time - start_time_).count();
}

auto DispatchFSM_::are_node_queues_empty() -> bool
{
    auto lock = node_registrar_.acquire();
    auto& nodes = lock->nodes();

    auto nonempty = true;

    for(const auto& n : nodes)
    {
        auto nl = n->acquire();

        const auto& st = nl->status;

        if(st.test_case_count != 0 || st.trace_count != 0)
        {
            nonempty = false;

            break;
        }
    }

    return nonempty;
}

auto DispatchFSM_::are_all_queues_empty() -> bool
{
    return
            are_node_queues_empty()
         && test_pool_.count_next() == 0
         && trace_pool_.count_next() == 0;
}

auto DispatchFSM_::are_nodes_inactive() -> bool
{
    auto lock = node_registrar_.acquire();
    auto& nodes = lock->nodes();

    auto inactive = true;

    for(const auto& n : nodes)
    {
        auto nl = n->acquire();

        const auto& st = nl->status;

        if(st.active)
        {
            inactive = false;

            break;
        }
    }

    {
        auto vmns_lock = vm_node_fsms_.acquire();

        std::for_each(vmns_lock->begin(),
                      vmns_lock->end(),
                      [&] (VMNodeFSM& nfsm)
        {
            if(nfsm->is_flag_active<vm::flag::active>())
            {
                inactive = false;

                return;
            }
        });
    }

    {
        auto svmns_lock = svm_node_fsms_.acquire();

        std::for_each(svmns_lock->begin(),
                      svmns_lock->end(),
                      [&] (SVMNodeFSM& nfsm)
        {
            if(nfsm->is_flag_active<svm::flag::active>())
            {
                inactive = false;

                return;
            }
        });
    }

    return inactive;
}

auto DispatchFSM_::is_converged() -> bool
{
    return
               are_all_queues_empty()
            && are_nodes_inactive();
}

auto DispatchFSM_::write_target_log(const log::NodeError& ne,
                                    const fs::path& subdir) -> void
{
    auto p = root_ / log_dir_name / subdir;

    {
        auto i = 1u;

        while(fs::exists(p / std::to_string(i)))
        {
            ++i;
        }

        p /= std::to_string(i);
    }

    fs::ofstream ofs{p};

    if(!ofs.good())
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{p.string()});
    }

    ofs << ne.log;
}


auto DispatchFSM_::test_pool() -> TestPool&
{
    return test_pool_;
}

auto DispatchFSM_::trace_pool() -> TracePool&
{
    return trace_pool_;
}

auto DispatchFSM_::set_up_root_dir() -> void
{
    auto timestamp_root = root_.filename();

    if(options_.mode.distributed)
    {
        auto target = fs::path{next_target_queue_.front()}.filename();

        if(root_.parent_path().filename() == dispatch_root_dir_name)
        {
            timestamp_root = root_.filename();
            root_ /= target;
        }
        else
        {
            timestamp_root = root_.parent_path().filename();
            root_ = root_.parent_path() / target;
        }
    }

    auto create_dirs = [](const fs::path& root)
    {
        return [root](const std::vector<std::string>& v)
        {
            for(const auto& e : v)
            {
                auto d = root / e;
                if(!fs::create_directories(d))
                {
                    BOOST_THROW_EXCEPTION(Exception{} << err::file_create{d.string()});
                }
            }
        };
    };

    if(!fs::exists(root_))
    {
        auto create_root_dirs = create_dirs(root_);
        auto create_log_dirs = create_dirs(root_ / log_dir_name);

        create_root_dirs({dispatch_trace_dir_name,
                          dispatch_test_case_dir_name,
                          dispatch_profile_dir_name,
                          dispatch_guest_data_dir_name});
        create_log_dirs({dispatch_log_vm_dir_name,
                         dispatch_log_svm_dir_name});
    }

    auto last_symlink = fs::path{dispatch_root_dir_name} / dispatch_last_root_symlink;

    fs::remove(last_symlink);
    fs::create_symlink(timestamp_root,
                       last_symlink);
}

auto DispatchFSM_::display_status(std::ostream& os) -> void
{
    using namespace std;

    system("clear");

    os << setw(12) << "time (s)"
         << "|"
         << setw(12) << "tests left"
         << "|"
         << setw(12) << "traces left"
         << "|";

    {
        auto count = 1u;
        auto lock = node_registrar_.acquire();
        for(const auto& node : lock->nodes())
        {
            auto tt = std::string{};

            tt += to_string(count++);

            if(node->acquire()->type == packet_type::cluster_request_vm_node)
                tt += "-[vm]";
            else
                tt += "-[svm]";

            tt += " tc/tr";

            os << setw(14) << tt
                 << "|";
        }
    }

    os << endl;

    auto disp_time = std::to_string(elapsed_time());

    auto test = to_string(test_pool_.count_next()) +
                 "/" +
                 to_string(test_pool_.count_all());
    auto trace = to_string(trace_pool_.count_next()) +
                 "/" +
                 to_string(trace_pool_.count_all_unique());

    os << setw(12) << disp_time
         << "|"
         << setw(12) << test
         << "|"
         << setw(12) << trace
         << "|";

    {
        auto lock = node_registrar_.acquire();
        for(const auto& node : lock->nodes())
        {
            auto tt = std::string{};
            auto lock = node->acquire();

            tt += to_string(lock->status.test_case_count) +
                  "/" +
                  to_string(lock->status.trace_count);
            os << setw(14) << tt
                 << "|";
        }
    }

    os << endl;
}

auto DispatchFSM_::write_tc_tree(std::ostream& os) -> void
{
    test_pool_.write_tc_tree(os);
}

auto DispatchFSM_::write_statistics() -> void
{
    static auto prev_time = decltype(elapsed_time()){0};
    static auto print_pg = true;
    auto time = elapsed_time();
    auto dir = root_ / dispatch_profile_dir_name;

    if(time - prev_time >= options_.profile.interval)
    {
        prev_time = time;
    }
    else
    {
        return;
    }

    if(print_pg)
    {
        print_pg = false;

        fs::ofstream ofs{dir / "stat.pg"};

        ofs << R"(#!/usr/bin/gnuplot
               reset
               set terminal png

               set title "Test cases and traces per second"
               set grid
               set key reverse Left outside
               set style data linespoints

               set ylabel "tcs/traces"

               set xlabel "seconds"

               plot "stat.dat" using 1:2 title "tc remaining", \
               "" using 1:3 title "tc total", \
               "" using 1:4 title "trace remaining", \
               "" using 1:5 title "trace total"
               #)";
    }

    fs::ofstream ofs{dir / "stat.dat"
                    ,std::ios_base::app};

    auto tc_left = test_pool_.count_next();
    auto tc_total = test_pool_.count_all();
    auto trace_left = trace_pool_.count_next();
    auto trace_total = trace_pool_.count_all_unique();

    ofs << time
        << " "
        << tc_left
        << " "
        << tc_total
        << " "
        << trace_left
        << " "
        << trace_total
        << "\n";
}

auto DispatchFSM_::node_registrar() -> AtomicGuard<NodeRegistrar>&
{
    return node_registrar_;
}

class DispatchFSM : public boost::msm::back::state_machine<DispatchFSM_>
{
};

} // namespace fsm


// +--------------------------------------------------+
// + Dispatch                                         +
// +--------------------------------------------------+

Dispatch::Dispatch(Port master,
                   const option::Dispatch& options)
    : dispatch_fsm_{std::make_shared<fsm::DispatchFSM>()}
{
    start_FSM(master,
              options);
}

auto Dispatch::run() -> bool
{
    bool ret = true;

    // TODO: Is this appropriate here? Wouldn't it be better to leave the decision to act to the FSM?
    if(!has_nodes() && !dispatch_fsm_->is_flag_active<fsm::flag::terminated>())
        return ret;

    if(dispatch_fsm_->is_flag_active<fsm::flag::terminated>())
    {
        ret = false;
    }
    else
    {
        dispatch_fsm_->process_event(fsm::poll{});
    }

    return ret;
}

auto Dispatch::has_nodes() -> bool
{
    return !dispatch_fsm_->node_registrar().acquire()->nodes().empty();
}

auto Dispatch::start_FSM(Port master,
                         const option::Dispatch& options) -> void
{
    dispatch_fsm_->start();

    dispatch_fsm_->process_event(fsm::start{master,
                                            options});
}

auto filter_vm(const NodeRegistrar::Nodes& nodes) -> NodeRegistrar::Nodes
{
    auto res = decltype(nodes){};

    std::copy_if(nodes.begin(),
                 nodes.end(),
                 std::back_inserter(res),
                 [](const NodeRegistrar::Node& node) {
        return node->acquire()->type != packet_type::cluster_request_vm_node;
    });

    return res;
}

auto filter_svm(const NodeRegistrar::Nodes& nodes) -> NodeRegistrar::Nodes
{
    auto res = decltype(nodes){};

    std::copy_if(nodes.begin(),
                 nodes.end(),
                 std::back_inserter(res),
                 [](const NodeRegistrar::Node& node) {
        return node->acquire()->type != packet_type::cluster_request_svm_node;
    });

    return res;
}

auto sort_by_trace(NodeRegistrar::Nodes& nodes) -> void
{
    std::sort(nodes.begin(),
              nodes.end(),
              [](const NodeRegistrar::Node& lhs,
                 const NodeRegistrar::Node& rhs) {
        return lhs->acquire()->status.trace_count < rhs->acquire()->status.trace_count;
    });
}

auto sort_by_test(NodeRegistrar::Nodes& nodes) -> void
{
    std::sort(nodes.begin(),
              nodes.end(),
              [](const NodeRegistrar::Node& lhs,
                 const NodeRegistrar::Node& rhs) {
        return lhs->acquire()->status.test_case_count < rhs->acquire()->status.test_case_count;
    });
}

auto receive_trace(NodeRegistrar::Node& node,
                   const fs::path& traces_dir) -> fs::path
{
    auto lock = node->acquire();

    auto pkinfo = PacketInfo{0,0,0};
    pkinfo.id = lock->status.id;
    pkinfo.type = packet_type::cluster_trace_request;

    lock->server.write(pkinfo);

    auto trace_name = std::string{};

    read_serialized_binary(lock->server,
                           trace_name,
                           packet_type::cluster_trace);

    auto trace = traces_dir / trace_name;

    {
        fs::ofstream ofs{trace,
                         std::ios::out | std::ios::binary};

        CRETE_EXCEPTION_ASSERT(ofs.good(),
                               err::file_open_failed{trace.string()});

        read(lock->server,
             ofs);
    }

    restore_directory(trace);

    return trace;
}

auto receive_tests(NodeRegistrar::Node& node) -> std::vector<TestCase>
{
    auto lock = node->acquire();

    auto pkinfo = PacketInfo{0,0,0};
    pkinfo.id = lock->status.id;
    pkinfo.type = packet_type::cluster_test_case_request;

    lock->server.write(pkinfo);

    auto tcs = std::vector<TestCase>{};

    read_serialized_binary(lock->server,
                           tcs,
                           packet_type::cluster_test_case);

    return tcs;
}

auto receive_errors(NodeRegistrar::Node& node) -> std::vector<log::NodeError>
{
    auto lock = node->acquire();

    auto pkinfo = PacketInfo{0,0,0};
    pkinfo.id = lock->status.id;
    pkinfo.type = packet_type::cluster_error_log_request;

    lock->server.write(pkinfo);

    auto errs = std::vector<log::NodeError>{};

    read_serialized_binary(lock->server,
                           errs,
                           packet_type::cluster_test_case);

    return errs;
}

auto receive_image_info(NodeRegistrar::Node& node) -> ImageInfo
{
    auto pkinfo = PacketInfo{0,0,0};
    auto image_info = ImageInfo{};
    auto lock = node->acquire();

    pkinfo.id = lock->status.id;
    pkinfo.size = 0;
    pkinfo.type = packet_type::cluster_image_info_request;

    lock->server.write(pkinfo);

    read_serialized_binary(lock->server,
                           image_info,
                           packet_type::cluster_image_info);

    return image_info;
}

auto transmit_trace(NodeRegistrar::Node& node,
                    const fs::path& trace) -> void
{
    auto lock = node->acquire();

    auto pkinfo = PacketInfo{0,0,0};
    pkinfo.id = lock->status.id;
    pkinfo.type = packet_type::cluster_trace;

    archive_directory(trace);

    fs::ifstream ifs{trace,
                     std::ios::in | std::ios::binary};

    CRETE_EXCEPTION_ASSERT(ifs.good(),
                           err::file_open_failed{trace.string()});

    write_serialized_binary(lock->server,
                            pkinfo,
                            trace.filename().string());

    write(lock->server,
          ifs,
          default_chunk_size);
}

auto transmit_tests(NodeRegistrar::Node& node,
                   const std::vector<TestCase>& tcs) -> void
{
    if(tcs.empty())
    {
        return;
    }

    auto lock = node->acquire();

    auto pkinfo = PacketInfo{0,0,0};
    pkinfo.id = lock->status.id;
    pkinfo.type = packet_type::cluster_test_case;

    write_serialized_binary(lock->server,
                            pkinfo,
                            tcs);
}

auto transmit_commencement(NodeRegistrar::Node& node) -> void
{
    auto lock = node->acquire();
    lock->server.write(lock->status.id,
                       packet_type::cluster_commence);
}

auto transmit_image_info(NodeRegistrar::Node& node,
                         const ImageInfo& ii) -> void
{
    auto lock = node->acquire();

    auto pkinfo = PacketInfo{0,0,0};
    pkinfo.id = lock->status.id;
    pkinfo.type = packet_type::cluster_image_info;

    write_serialized_binary(lock->server,
                            pkinfo,
                            ii);
}

auto transmit_config(NodeRegistrar::Node& node,
                     const option::Dispatch& options) -> void
{
    auto lock = node->acquire();

    auto pkinfo = PacketInfo{0,0,0};
    pkinfo.id = lock->status.id;
    pkinfo.type = packet_type::cluster_config;

    write_serialized_binary(lock->server,
                            pkinfo,
                            options);

}

auto register_node_fsm(NodeRegistrar::Node& node,
                       const option::Dispatch& options,
                       const fs::path& root,
                       AtomicGuard<fsm::DispatchFSM::VMNodeFSMs>& vm_node_fsms,
                       AtomicGuard<fsm::DispatchFSM::SVMNodeFSMs>& svm_node_fsms) -> void
{
    node->acquire()->status.active = true;

    switch(node->acquire()->type)
    {
    case packet_type::cluster_request_vm_node:
    {
        auto first_vm_node = vm_node_fsms.acquire()->empty();
        auto fsm = std::make_shared<vm::NodeFSM>();
        fsm->start();
        fsm->process_event(vm::start{node,
                                     first_vm_node,
                                     root / dispatch_trace_dir_name,
                                     options.vm.image.update,
                                     options.mode.distributed});
        vm_node_fsms.acquire()->emplace_back(fsm);
        break;
    }
    case packet_type::cluster_request_svm_node:
    {
        auto fsm = std::make_shared<svm::NodeFSM>();
        fsm->start();
        fsm->process_event(svm::start{node});
        svm_node_fsms.acquire()->emplace_back(fsm);
        break;
    }
    default:
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::msg{"node type not recognized"});
        break;
    }
    }
}

auto make_dispatch_root() -> boost::filesystem::path
{
    using namespace boost::posix_time;
    using namespace boost::gregorian;

    auto p = fs::path{dispatch_root_dir_name};

    auto now = second_clock::local_time();
    auto s = to_simple_string(now);

    std::replace(s.begin(), s.end(), ' ', '_');
    std::replace(s.begin(), s.end(), ':', '.');

    return p / s;
}

auto extract_initial_test(const config::RunConfiguration& config) -> TestCase
{
    using namespace crete::config;

    const Arguments& args = config.get_arguments();
    const Files& files = config.get_files();
    const STDStream& concolic_stdin = config.get_stdin();

    std::cout << "args.size: " << args.size() << std::endl;
    std::cout << "file.size: " << files.size() << std::endl;

    // Note: order of test case elements matters.

    TestCase tc;

    for(Arguments::const_iterator it = args.begin();
        it != args.end();
        ++it)
    {
        std::stringstream ss_name;
        ss_name << "argv_" << it->index;
        const Argument& arg = *it;

        if(!arg.concolic)
        {
            continue;
        }

        std::string name = ss_name.str(); // Must match that of preload.

        TestCaseElement elem;

        elem.name = std::vector<uint8_t>(name.begin(), name.end());
        elem.name_size = name.size();

        if(!arg.value.empty())
        {
            elem.data = std::vector<uint8_t>(arg.value.begin(), arg.value.end());
        }

        if(elem.data.size() < arg.size) {
            elem.data.resize(arg.size, 0);
        }

        elem.data_size = elem.data.size();

        tc.add_element(elem);
    }

    for(Files::const_iterator it = files.begin();
        it != files.end();
        ++it)
    {
        if(!it->concolic)
            continue;

        const File& file = *it;

        TestCaseElement elem;

        std::string name = file.path.filename().string();

        elem.name = std::vector<uint8_t>(name.begin(), name.end());
        elem.name_size = name.size();

        elem.data = file.data;
        elem.data_size = elem.data.size();
        assert(elem.data_size == file.size);

        tc.add_element(elem);

        TestCaseElement elem_posix = elem;
        std::string posix_name = name + "-posix";
        elem_posix.name = std::vector<uint8_t>(posix_name.begin(), posix_name.end());
        elem_posix.name_size = posix_name.size();

        tc.add_element(elem_posix);
    }

    if(concolic_stdin.concolic)
    {
        TestCaseElement elem;

        elem.data = std::vector<uint8_t>(concolic_stdin.value.begin(),
                concolic_stdin.value.end());
        elem.data_size = elem.data.size();
        assert(elem.data_size == concolic_stdin.size);

        std::string name = "crete-stdin";
        elem.name = std::vector<uint8_t>(name.begin(), name.end());
        elem.name_size = elem.name.size();

        tc.add_element(elem);

        name = "crete-stdin-posix";
        elem.name = std::vector<uint8_t>(name.begin(), name.end());
        elem.name_size = name.size();

        tc.add_element(elem);
    }

    if(tc.get_elements().size() == 0)
    {
        std::cerr << "[vm_node] Warning: no test case elements deduced from guest configuration file\n";
    }

    return tc;
}

} // namespace cluster
} // namespace crete
