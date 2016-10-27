#ifndef CRETE_SERVER_H
#define CRETE_SERVER_H

#include <string>
#include <iostream>

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/filesystem/path.hpp>

#include <crete/asio/common.h>

namespace crete
{

class Server
{
public:
    Server(Port port);
    Server(); // Select random available port.
    ~Server();

    void write_message(const std::string& msg);
    std::string read_message();
    size_t write(const std::vector<char>& buf,
                 const PacketInfo& pkinfo);
    void write(boost::asio::streambuf& sbuf,
               const PacketInfo& pktinfo);
    void write(boost::asio::streambuf& sbuf,
               const uint64_t& id,
               const uint32_t& type);
    void write(const uint64_t& id,
               const uint32_t& type);
    void write(const PacketInfo& pktinfo);
//    size_t write(const boost::asio::buffer& buf,
//                 const PacketInfo& pktinfo);
    PacketInfo read(std::vector<char>& buf);
    PacketInfo read(boost::asio::streambuf& sbuf);
    PacketInfo read();

    /// Handler signature: void handler(const boost::system::error_code&);
    template <typename Handler>
    void open_connection_async(Handler handler);
    void open_connection_wait();
    void open_connection_wait(boost::posix_time::time_duration timeout);
    bool is_socket_open(); // Will return true even if connection has been closed.

    void update_directory(const boost::filesystem::path& from, const boost::filesystem::path& to);

    Port port() const;

protected:
    std::vector<std::string> query_directory_files(const UpdateQuery& update_query);
    void use_available_port();


private:
    boost::asio::io_service io_service_;
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::ip::tcp::socket socket_;
    Port port_;
};

template <typename Handler>
void Server::open_connection_async(Handler handler)
{
    acceptor_.async_accept(socket_,
                           handler);
}

// Deprecated: unsafe.
//uint16_t find_available_port();

} // namespace crete

#endif // CRETE_SERVER_H
