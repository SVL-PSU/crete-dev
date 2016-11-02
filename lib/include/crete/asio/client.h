#ifndef CRETE_CLIENT_H
#define CRETE_CLIENT_H

#include <string>
#include <vector>

#include <boost/asio.hpp>

#include <crete/asio/common.h>

namespace crete
{

class ClientImpl;

class Client
{
public:
    Client(const std::string& host_ip, const std::string& port);
    ~Client();

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

private:
    ClientImpl* pimpl_;
};

}

#endif // CRETE_CLIENT_H
