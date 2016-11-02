#ifndef CRETE_CLIENT_IMPL_H
#define CRETE_CLIENT_IMPL_H

/*
 * Using pimpl idiom because conflicts with #define name barrier() in QEMU with boost threading code
 */

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <string>

#include <crete/asio/common.h>

namespace crete
{

class ClientImpl
{
public:
    ClientImpl(const std::string& host_ip, const std::string& port);
    ~ClientImpl();

    void write_message(const std::string& msg);
    std::string read_message();
    size_t write(const std::vector<char>& buf,
                 const PacketInfo& pkinfo);
    void write(boost::asio::streambuf& sbuf, const PacketInfo& pktinfo);
    void write(boost::asio::streambuf& sbuf,
               const uint64_t& id,
               const uint32_t& type);
    void write(const uint64_t& id,
               const uint32_t& type);
    void write(const PacketInfo& pktinfo);
    PacketInfo read(std::vector<char>& buf);
    PacketInfo read(boost::asio::streambuf& sbuf);
    PacketInfo read();
    PacketInfo read(boost::posix_time::time_duration timeout);

    void connect();
    void connect(boost::posix_time::time_duration timeout);

protected:
    void initialize_deadline_timer();
    void check_deadline();
    void reset_deadline_timer();

private:
    boost::asio::io_service io_service_;
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::ip::tcp::resolver::query query_;
    boost::asio::ip::tcp::resolver::iterator endpoint_iterator_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::deadline_timer deadline_timer_;
};

} // namespace crete

#endif // CRETE_CLIENT_IMPL_H
