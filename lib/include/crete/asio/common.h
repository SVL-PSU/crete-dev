#ifndef CRETE_ASIO_COMMON_H
#define CRETE_ASIO_COMMON_H

#ifndef __STDC_LIMIT_MACROS_MACROS
#define __STDC_LIMIT_MACROS_MACROS
#endif

#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include <boost/serialization/map.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>

#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ptree_serialization.hpp>

#include <boost/filesystem.hpp>
#include <boost/asio.hpp>

#include <crete/util/util.h>
#include <crete/exception.h>

#include <map>
#include <string>

#include <stdint.h>

namespace crete
{

const size_t asio_max_msg_size = 32;
const uint32_t default_chunk_size = 1024;

typedef unsigned short Port;
typedef std::string IPAddress;

namespace packet_type // Not an enum b/c pre-C++11 enums size types are impl-defined.
{
const uint32_t properties = 0;
const uint32_t update_query = 1;
const uint32_t elf_entries = 2;
const uint32_t guest_configuration = 3;
const uint32_t proc_maps = 4;
const uint32_t cluster_request_vm_node = 5;
const uint32_t cluster_request_svm_node = 6;
const uint32_t cluster_port = 7;
const uint32_t cluster_status_request = 8;
const uint32_t cluster_status = 9;
const uint32_t cluster_shutdown = 10;
const uint32_t cluster_trace = 11;
const uint32_t cluster_port_request = 12;
const uint32_t cluster_next_test = 13;
const uint32_t cluster_test_case = 14;
const uint32_t cluster_trace_request = 15;
const uint32_t cluster_test_case_request = 16;
const uint32_t cluster_config = 18;
const uint32_t cluster_image_info_request = 19;
const uint32_t cluster_image_info = 20;
const uint32_t cluster_image = 21;
const uint32_t cluster_commence = 22;
const uint32_t cluster_reset = 23;
const uint32_t cluster_next_target = 24;
const uint32_t cluster_error_log_request = 25;
const uint32_t cluster_error_log = 26;
const uint32_t chunk = 27;
const uint32_t file_stream = 28;
const uint32_t cluster_request_guest_data = 29;
const uint32_t cluster_tx_guest_data = 30;
const uint32_t cluster_request_guest_data_post_exec = 31;
const uint32_t cluster_tx_guest_data_post_exec = 32;
}

struct PacketInfo
{
    uint64_t id; // Use: implementation defined. Meant as supplementary to 'type'.
    uint64_t size; // Size of the associated packet.
    uint64_t type; // Type of the associated packet.
};

template <typename Connection, typename T>
void write_serialized_text(Connection& connection, PacketInfo& pktinfo, T& t)
{
    boost::asio::streambuf sbuf;
    std::ostream os(&sbuf);

    boost::archive::text_oarchive oa(os);
    oa << t;

    pktinfo.size = sbuf.size();

    connection.write(sbuf, pktinfo);
}

template <typename Connection, typename T>
void write_serialized_binary(Connection& connection, PacketInfo& pktinfo, T& t)
{
    boost::asio::streambuf sbuf;
    std::ostream os(&sbuf);

    boost::archive::binary_oarchive oa(os);
    oa << t;

    pktinfo.size = sbuf.size();

    connection.write(sbuf, pktinfo);
}

template <typename Connection>
void write_serialized_text(
        Connection& connection,
        PacketInfo& pktinfo,
        const boost::property_tree::ptree& t,
        size_t version)
{
    boost::asio::streambuf sbuf;
    std::ostream os(&sbuf);

    boost::archive::text_oarchive oa(os);
    boost::property_tree::save(oa, t, version);

    pktinfo.size = sbuf.size();

    connection.write(sbuf, pktinfo);
}

template <typename Connection, typename T>
void write_serialized_text_xml(Connection& connection, PacketInfo& pktinfo, T& t)
{
    boost::asio::streambuf sbuf;
    std::ostream os(&sbuf);

    boost::archive::xml_oarchive oa(os);
    oa << BOOST_SERIALIZATION_NVP(t);

    pktinfo.size = sbuf.size();

    connection.write(sbuf, pktinfo);
}

template <typename Connection, typename T>
void read_serialized_text(Connection& connection, T& t)
{
    boost::asio::streambuf sbuf;
    connection.read(sbuf);

    std::istream is(&sbuf);
    boost::archive::text_iarchive ia(is);

    ia >> t;
}

template <typename Connection, typename T>
void read_serialized_text(Connection& connection,
                          T& t,
                          uint32_t pk_type)
{
    boost::asio::streambuf sbuf;
    PacketInfo pkinfo = connection.read(sbuf);

    CRETE_EXCEPTION_ASSERT(pkinfo.type == pk_type,
                           err::network_type_mismatch(pkinfo.type));

    std::istream is(&sbuf);
    boost::archive::text_iarchive ia(is);

    ia >> t;
}

template <typename T>
void read_serialized_text(boost::asio::streambuf& sbuf,
                          T& t)
{
    std::istream is(&sbuf);
    boost::archive::text_iarchive ia(is);

    ia >> t;
}

template <typename T>
void read_serialized_binary(boost::asio::streambuf& sbuf,
                          T& t)
{
    std::istream is(&sbuf);
    boost::archive::binary_iarchive ia(is);

    ia >> t;
}

template <typename Connection, typename T>
void read_serialized_binary(Connection& connection,
                            T& t,
                            uint32_t pk_type)
{
    boost::asio::streambuf sbuf;
    PacketInfo pkinfo = connection.read(sbuf);

    CRETE_EXCEPTION_ASSERT(pkinfo.type == pk_type,
                           err::network_type_mismatch(pkinfo.type));

    std::istream is(&sbuf);
    boost::archive::binary_iarchive ia(is);

    ia >> t;
}


template <typename Connection, typename T>
void read_serialized_text_xml(Connection& connection, T& t)
{
    boost::asio::streambuf sbuf;
    connection.read(sbuf);

#if 0 // Debugging: Write contents to file instead.
    std::istream ist(&sbuf);
    std::ofstream ofs("config.ser");
    std::copy(std::istreambuf_iterator<char>(ist),
              std::istreambuf_iterator<char>(),
              std::ostream_iterator<char>(ofs));
#endif

    std::istream is(&sbuf);
    boost::archive::xml_iarchive ia(is);

    ia >> BOOST_SERIALIZATION_NVP(t);
}


template <typename Connection, typename T>
void read_serialized_text_xml(Connection& connection,
                              T& t,
                              uint32_t pk_type)
{
    boost::asio::streambuf sbuf;
    PacketInfo pkinfo = connection.read(sbuf);

    CRETE_EXCEPTION_ASSERT(pkinfo.type == pk_type,
                           err::network_type_mismatch(pkinfo.type));

    std::istream is(&sbuf);
    boost::archive::xml_iarchive ia(is);

    ia >> BOOST_SERIALIZATION_NVP(t);
}

template <typename Connection>
void read_serialized_text(
        Connection& connection,
        boost::property_tree::ptree& t)
{
    boost::asio::streambuf sbuf;
    connection.read(sbuf);

    std::istream is(&sbuf);
    boost::archive::text_iarchive ia(is);

    boost::property_tree::load(ia, t, 0);
}

/**
 * @brief Writes entire contents of stream, is, to destination represented by 'connection'
 *        incrementally based on 'chunk_size.' Enables "streaming" of files across connections.
 *
 * @param connection represents a client or server connection
 * @param is stream with data to send
 * @param chunk_size amount of data to send at each increment. Pay careful attention here to avoid
 *        resource issues. Each packet can only be reasonably expected to send a certain amount of data.
 *
 * @pre connection represents a valid condition
 * @post is.tellg == std::ios::end
 *
 * @note 1. Chunks are not compressed. Future experimentation should be done to determine if doing so
 *          would be more efficient. It would be very simple to integrate with boost::iostreams compression.
 * @note 2. It would be nice if this took an optional logger to report on progress (% sent), for very large
 *          files that take an exceedingly long time (e.g., OS images).
 */
template<typename Connection>
void write(Connection& connection,
           std::istream& is,
           std::size_t chunk_size)
{
    std::vector<char> buf;
    std::size_t count = 0;
    std::streamsize bytes_read = 0;
    std::istream::pos_type stream_size = util::stream_size(is);

    // TODO: C++11: std::numeric_limits<decltype(chunk_size)>::max >= std::numeric_limits<std::vector::size_type>::max();
    // FIXME: replace INT_MAX with UINT32_MAX
    CRETE_EXCEPTION_ASSERT(chunk_size <= UINT_MAX, err::unsigned_to_signed_conversion(chunk_size));
    buf.resize(chunk_size);

    CRETE_EXCEPTION_ASSERT(stream_size > 0, err::stream_size(stream_size));

    {
        PacketInfo pkinfo;
        pkinfo.id = 0;
        pkinfo.type = packet_type::file_stream;
        // TODO: C++11: static_cast<decltype(pkinfo.size)>(file_size);
        pkinfo.size = static_cast<uint64_t>(stream_size);

        connection.write(pkinfo);
    }

    do
    {
        is.read(buf.data(), static_cast<std::streamsize>(buf.size()));

        bytes_read = is.gcount();

        if(bytes_read > 0)
        {
            PacketInfo pkinfo;
            pkinfo.id = count;
            pkinfo.type = crete::packet_type::chunk;
            pkinfo.size = static_cast<uint32_t>(bytes_read);

            size_t bytes_written = connection.write(buf,
                                                    pkinfo);

            CRETE_EXCEPTION_ASSERT(bytes_written == static_cast<uint32_t>(bytes_read),
                                   err::network("failed to send requested number of bytes"));

            ++count;
        }
    }while(bytes_read > 0);
}

template<typename Connection>
void read(Connection& connection,
          std::ostream& os)
{
    PacketInfo pkinfo = connection.read();

    CRETE_EXCEPTION_ASSERT(pkinfo.type == packet_type::file_stream, err::network_type_mismatch(pkinfo.type));

    uint64_t stream_size = pkinfo.size;
    uint64_t bytes_written = 0;

    CRETE_EXCEPTION_ASSERT(stream_size > 0, err::network("stream size expected must be larger than 0"));

    do
    {
        std::vector<char> buf;

        pkinfo = connection.read(buf);

        CRETE_EXCEPTION_ASSERT(pkinfo.size >= 0, err::network("zero bytes received"));

        bytes_written += pkinfo.size;

        CRETE_EXCEPTION_ASSERT(bytes_written <= stream_size, err::msg("number of bytes sent exceeds number expected for streamed file"));

        // TODO: C++11: numeric_limits<std::streamsize>::max() rather than INT_MAX
        // FIXME: replace INT_MAX with UINT32_MAX
        CRETE_EXCEPTION_ASSERT(buf.size() <= UINT_MAX, err::unsigned_to_signed_conversion(buf.size()));

        assert(buf.size() == pkinfo.size);

        os.write(buf.data(), static_cast<std::streamsize>(buf.size()));

    }while(bytes_written != stream_size);

    os.flush(); // Seems to be a bug in gcc-4.6. Destructor doesn't always call close/flush.
}

typedef std::map<std::string, std::time_t> QueryFiles;

class UpdateQuery
{
public:
    UpdateQuery() {}
    UpdateQuery(const std::string& dir, const QueryFiles& qfs) : dir_(dir), qfs_(qfs) {}

    const std::string& directory() const { return dir_; }
    const QueryFiles& query_files() const { return qfs_; }

private:
    friend class boost::serialization::access;
    template <typename Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & dir_;
        ar & qfs_;
    }

    std::string dir_;
    QueryFiles qfs_;
};

class FileInfo
{
public:
    FileInfo(std::string name, uintmax_t size, boost::filesystem::file_type type, boost::filesystem::perms perms) :
        name_(name), size_(size), type_(type), perms_(perms) {}

    std::string path() const { return name_; }
    uintmax_t size() const { return size_; }
    boost::filesystem::file_type type() const { return type_; }
    boost::filesystem::perms perms() const { return perms_; }

private:
    std::string name_;
    uintmax_t size_;
    boost::filesystem::file_type type_;
    boost::filesystem::perms perms_;

    friend class boost::serialization::access;
    template <typename Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & name_;
        ar & size_;
        ar & type_;
        ar & perms_;
    }
};

class FileTransferList
{
public:
    void push_back(const FileInfo& info) { files_.push_back(info); }

    const std::vector<FileInfo> files() { return files_; }

private:
    std::vector<FileInfo> files_;

    friend class boost::serialization::access;
    template <typename Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & files_;
    }
};

} // namespace crete

#endif // CRETE_ASIO_COMMON_H
