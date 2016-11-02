/***
 * Author: Christopher Havlicek
 ***/

#ifndef CRETE_CLUSTER_COMMON_H
#define CRETE_CLUSTER_COMMON_H

#include <stdint.h>

#include <boost/filesystem/path.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_serialize.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <crete/asio/common.h>
#include <crete/asio/client.h>
#include <crete/run_config.h>

namespace crete
{
namespace cluster
{

using ID = uint64_t;

const auto bandwidth_in_bytes = uint64_t{125000000}; // 1 Gigabit in bytes
const auto image_name = std::string{"crete.img"};
const auto trace_dir_name = std::string{"trace"};
const auto hostfile_dir_name = std::string{"hostfile"};
const auto image_info_name = std::string{"crete.img.info"};
const auto input_args_name = std::string{"input_arguments.bin"};
const auto trace_ready_name = std::string{"trace_ready"};
const auto vm_port_file_name = std::string{"port"};
const auto vm_pid_file_name = std::string{"pid"};
const auto log_dir_name = std::string{"log"};
const auto exception_log_file_name = std::string{"exception_caught.log"};
const auto image_max_file_size = uint64_t{8000000000}; // 10 Gigabytes in bytes

struct NodeStatus
{
    uint64_t id = 0;
    uint32_t test_case_count = 0;
    uint32_t trace_count = 0;
    uint32_t error_count = 0; // Reported errors from node. To be retrieved, as tcs and traces.
    bool active = true; // Designates whether the node is currently doing things, or just waiting.

    template <typename Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & id;
        ar & test_case_count;
        ar & trace_count;
        ar & error_count;
        ar & active;
    }
};

struct ImageInfo
{
    std::string file_name_;
    int64_t last_write_time_{0u};

    ImageInfo() = default;
    ImageInfo(const boost::filesystem::path& image);

    template <typename Archive>
    auto serialize(Archive& ar, const unsigned int version) -> void
    {
        (void)version;

        ar & file_name_;
        ar & last_write_time_;
    }
};

inline auto operator==(const ImageInfo& lhs, const ImageInfo& rhs) -> bool
{
    return lhs.last_write_time_ == rhs.last_write_time_ &&
           lhs.file_name_ == rhs.file_name_;
}

inline auto operator!=(const ImageInfo& lhs, const ImageInfo& rhs) -> bool
{
    return !(lhs == rhs);
}

struct GuestData
{
    config::RunConfiguration guest_config;

    template <typename Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & guest_config;
    }

    auto write_guest_config(const boost::filesystem::path &output) -> void;
};

// Temporarily placed here until crete/trace.h can be updated or moved to a trace-analyzer specific file.
//struct Trace
//{
//    Trace(const boost::filesystem::path& p) : path(p) {}

//    boost::filesystem::path path;

//    template <typename Archive>
//    void save(Archive& ar, const unsigned int version) const
//    {
//        (void)version;

//        ar & path.string();
//    }
//    template <typename Archive>
//    void load(Archive& ar, const unsigned int version)
//    {
//        (void)version;

//        std::string tmp;

//        ar & tmp;

//        path = tmp;
//    }
//    BOOST_SERIALIZATION_SPLIT_MEMBER()
//};

struct Trace
{
    boost::uuids::uuid uuid_{{0}}; // Double brace for 'brace elision.'
    std::vector<uint8_t> data_; // Note: may be compressed.

    template <typename Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & uuid_;
        ar & data_;
    }
};

auto archive_directory(const boost::filesystem::path& dir) -> void;
auto restore_directory(const boost::filesystem::path& dir) -> void;

struct NodeRequest
{
    NodeRequest(PacketInfo& pkinfo,
                boost::asio::streambuf& sbuf,
                Client& client) :
        pkinfo_{pkinfo},
        sbuf_{sbuf},
        client_{client} {}

    PacketInfo& pkinfo_;
    boost::asio::streambuf& sbuf_;
    Client& client_;
};

namespace log
{

struct NodeError
{
    std::string log;
//    uint32_t node_type{0};
//    decltype(NodeStatus::id) id{0};
//    uint32_t error_type{0}; // TODO: enum class: fatal, nonfatal?

    template <typename Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & log;
//        ar & node_type;
//        ar & id;
//        ar & error_type;
    }
};

} // namespace log

} // namespace cluster
} // namespace crete

#endif // CRETE_CLUSTER_COMMON_H
