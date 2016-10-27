#include <crete/asio/server.h>
#include <crete/exception.h>

#include <iostream>

#include <boost/filesystem/operations.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/lambda/lambda.hpp>

using boost::asio::ip::tcp;
using namespace std;
namespace bl = boost::lambda;

namespace crete
{

Server::Server(Port port) :
    acceptor_(io_service_, tcp::endpoint(tcp::v6(), port)),
    socket_(io_service_),
    port_(port)
{
}

Server::Server() :
    acceptor_(io_service_),
    socket_(io_service_),
    port_(0)
{
    use_available_port();
}

Server::~Server()
{
    try
    {
        if(socket_.is_open()) // shutdown() throws exception on failure, so ensure it's open or std::terminate() will be called.
        {
            socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_send);
            socket_.close();
        }

        io_service_.stop();
    }
    catch(...) // TODO: Appropriate? If shutdown() or close() throw, std::terminate would be called because it's inside a dtor.
    {
    }
}

void Server::open_connection_wait()
{
    acceptor_.accept(socket_);
}

bool Server::is_socket_open()
{
    return socket_.is_open();
}

void Server::write_message(const std::string& msg)
{
    if(msg.size() >= asio_max_msg_size)
        throw runtime_error("asio message to big");    

    vector<char> buf(asio_max_msg_size);
    copy(msg.begin(), msg.end(), buf.begin());

    boost::system::error_code error;

    size_t nsent = boost::asio::write(socket_, boost::asio::buffer(buf, asio_max_msg_size), error);

    if(nsent != asio_max_msg_size)
        throw runtime_error("failed to send entire message");

    if(error)
        throw boost::system::system_error(error);
}

string Server::read_message()
{
    vector<char> buf(asio_max_msg_size);
    boost::system::error_code error;

    size_t nrec = boost::asio::read(socket_, boost::asio::buffer(buf, asio_max_msg_size), error);

    if(error == boost::asio::error::eof)
      return string("connection closed cleanly by peer"); // Connection closed cleanly by peer.
    else if(error)
      throw boost::system::system_error(error); // Some other error.

    return string(string(buf.begin(), buf.begin() + nrec).c_str());
}

size_t Server::write(const std::vector<char>& buf, const PacketInfo& pkinfo)
{
    write(pkinfo);

    CRETE_EXCEPTION_ASSERT(buf.size() >= pkinfo.size,
                           err::msg("buffer smaller than size to be read from"));

    return boost::asio::write(socket_,
                              boost::asio::buffer(buf.data(),
                                                  pkinfo.size));
}

void Server::write(boost::asio::streambuf& sbuf,
                     const PacketInfo& pktinfo)
{
    write(pktinfo);

    size_t nsent = boost::asio::write(socket_, sbuf);

    if(nsent != pktinfo.size)
        throw runtime_error("failed to send entire stream");
}

void Server::write(boost::asio::streambuf& sbuf,
                   const uint64_t& id,
                   const uint32_t& type)
{
    PacketInfo pkinfo;

    pkinfo.id = id;
    pkinfo.type = type;
    pkinfo.size = sbuf.size();

    write(sbuf, pkinfo);
}

void Server::write(const uint64_t& id,
                   const uint32_t& type)
{
    PacketInfo pkinfo;

    pkinfo.id = id;
    pkinfo.type = type;
    pkinfo.size = 0;

    write(pkinfo);
}

void Server::write(const PacketInfo& pktinfo)
{
    size_t nsent = boost::asio::write(socket_,
                                      boost::asio::buffer(reinterpret_cast<const uint8_t*>(&pktinfo),
                                                          sizeof(PacketInfo)));

    if(nsent != sizeof(PacketInfo))
        throw runtime_error("failed to send entire stream");
}

PacketInfo Server::read(std::vector<char>& buf)
{
    boost::system::error_code error;
    PacketInfo pktinfo;
    size_t nrec = 0;

    pktinfo = read();

    buf.resize(pktinfo.size);

    nrec = boost::asio::read(socket_,
                             boost::asio::buffer(buf),
                             error);

    if(error)
      throw boost::system::system_error(error);
    if(nrec != pktinfo.size)
        throw runtime_error("failed to receive expected number of bytes for packet");

    return pktinfo;
}

PacketInfo Server::read(boost::asio::streambuf& sbuf)
{
    boost::system::error_code error;
    PacketInfo pktinfo;
    size_t nrec = 0;

    pktinfo = read();

    nrec = boost::asio::read(socket_,
                             boost::asio::buffer(sbuf.prepare(pktinfo.size)),
                             error);

    if(error)
      throw boost::system::system_error(error);
    if(nrec != pktinfo.size)
        throw runtime_error("failed to receive expected number of bytes for packet");

    sbuf.commit(nrec);

    return pktinfo;
}

PacketInfo Server::read()
{
    boost::system::error_code error;
    PacketInfo pktinfo;
    size_t nrec = 0;

    nrec = boost::asio::read(socket_,
                             boost::asio::buffer(reinterpret_cast<uint8_t*>(&pktinfo),
                                                 sizeof(PacketInfo)),
                             error);

    if(error)
      throw boost::system::system_error(error); // Some other error.
    if(nrec != sizeof(PacketInfo))
        throw runtime_error("failed to receive expected number of bytes for packet header");

    return pktinfo;
}

void crete::Server::update_directory(const boost::filesystem::path& from, const boost::filesystem::path& to)
{
    namespace fs = boost::filesystem;

    if(!fs::exists(from))
        throw runtime_error("can't find source directory: " + from.generic_string());
    if(!fs::is_directory(from))
        throw runtime_error("not a directory: " + from.generic_string());

    QueryFiles to_query;

    fs::directory_iterator end;
    for(fs::directory_iterator iter(from); iter != end; ++iter)
    {
        const fs::path& p = iter->path();
        to_query.insert(make_pair(p.filename().generic_string(),
                                  fs::last_write_time(p)));
    }

    // Sends a serialized class representing this map, and waits for the client to respond with the
    // out of date files. From there, we send them (in binary form).
    vector<string> to_update = query_directory_files(UpdateQuery(to.generic_string(), to_query));

    for(vector<string>::iterator iter = to_update.begin();
        iter != to_update.end();
        ++iter)
    {
        string name = *iter;
        cout << "sending: "  << name << endl;

        fs::path file_path = to/name;
        uintmax_t size = fs::file_size(file_path);
        assert(size != uintmax_t(-1));
        fs::file_status status = fs::status(file_path);
        assert(status.type() == fs::regular_file);
        fs::file_type type = status.type();
        fs::perms perms = status.permissions();

        FileInfo finfo(
                    name,
                    size,
                    type,
                    perms);

        // Start from here. Need to package this all up, and send it to the client.
        // Then the client needs to figure out what to do with it.
    }
}

Port Server::port() const
{
    return port_;
}

vector<string> Server::query_directory_files(const UpdateQuery& update_query)
{
    PacketInfo pkinfo;
    pkinfo.id = 0;
    pkinfo.type = packet_type::update_query;
    write_serialized_text(*this, pkinfo, update_query);

    vector<string> out_of_dates;
    read_serialized_text(*this, out_of_dates);

    return out_of_dates;
}

void Server::use_available_port()
{
    uint16_t port = 0;

    tcp::endpoint epoint(tcp::endpoint(boost::asio::ip::tcp::v6(),
                                       port)); // Port of 0 causes an available port to be selected.
    acceptor_.open(epoint.protocol());
    acceptor_.set_option(tcp::acceptor::reuse_address(true));
    acceptor_.bind(epoint);
    acceptor_.listen();

    tcp::endpoint le = acceptor_.local_endpoint();
    port = le.port();

    assert(port != 0);

    port_ = port;
}

// Deprecated: unsafe.
//uint16_t find_available_port()
//{
//    boost::asio::io_service service;
//    tcp::acceptor acceptor(service);
//    uint16_t port = 0;

//    tcp::endpoint epoint(tcp::endpoint(boost::asio::ip::tcp::v6(),
//                                       port));
//    acceptor.open(epoint.protocol());
//    acceptor.set_option(tcp::acceptor::reuse_address(true));
//    acceptor.bind(epoint);

//    tcp::endpoint le = acceptor.local_endpoint();
//    port = le.port();

//    assert(port != 0);

//    acceptor.close();

//    return port;
//}

} // namespace crete
