#include "client_pimpl.h"
#include <crete/asio/common.h>

#include <boost/asio.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/lambda/lambda.hpp>

#include <vector>
#include <iterator>
#include <algorithm>

using namespace std;
namespace bl = boost::lambda;

namespace crete
{

ClientImpl::ClientImpl(const std::string& host_ip, const std::string& port) :
    resolver_(io_service_),
    query_(host_ip, port),
    endpoint_iterator_(resolver_.resolve(query_)),
    socket_(io_service_),
    deadline_timer_(io_service_)
{
}

ClientImpl::~ClientImpl()
{
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_send);
    socket_.close();
    io_service_.stop();
}

void ClientImpl::connect()
{
    boost::asio::connect(socket_, endpoint_iterator_);
}

void ClientImpl::connect(boost::posix_time::time_duration timeout)
{

    deadline_timer_.expires_from_now(timeout);

    boost::system::error_code ec = boost::asio::error::would_block;

    boost::asio::async_connect(socket_,
                         endpoint_iterator_,
                         boost::lambda::var(ec) = boost::lambda::_1);

    do
    {
        io_service_.run_one();

    } while(ec == boost::asio::error::would_block);

    if (ec || !socket_.is_open())
          throw boost::system::system_error(
              ec ? ec : boost::asio::error::operation_aborted);

    reset_deadline_timer();
}

void ClientImpl::initialize_deadline_timer()
{
    deadline_timer_.expires_at(boost::posix_time::pos_infin);
    check_deadline();
}

void ClientImpl::check_deadline()
{
    if(deadline_timer_.expires_at()
       <=
       boost::asio::deadline_timer::traits_type::now())
    {
        boost::system::error_code ec;

        socket_.close(ec);

        deadline_timer_.expires_at(boost::posix_time::pos_infin);
    }

    deadline_timer_.async_wait(bl::bind(&ClientImpl::check_deadline, this));
}

void ClientImpl::reset_deadline_timer()
{
    deadline_timer_.expires_at(boost::posix_time::pos_infin);
}

void ClientImpl::write_message(const std::string& msg)
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

string ClientImpl::read_message()
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

size_t ClientImpl::write(const std::vector<char>& buf, const PacketInfo& pkinfo)
{
    write(pkinfo);

    CRETE_EXCEPTION_ASSERT(buf.size() >= pkinfo.size,
                           err::msg("buffer smaller than size to be read from"));

    return boost::asio::write(socket_,
                              boost::asio::buffer(buf.data(),
                                                  pkinfo.size));
}

void ClientImpl::write(boost::asio::streambuf& sbuf, const PacketInfo& pktinfo)
{
    write(pktinfo);

    size_t nsent = boost::asio::write(socket_, sbuf);

    if(nsent != pktinfo.size)
        throw runtime_error("failed to send entire stream");
}

void ClientImpl::write(boost::asio::streambuf& sbuf,
                   const uint64_t& id,
                   const uint32_t& type)
{
    PacketInfo pkinfo;

    pkinfo.id = id;
    pkinfo.type = type;
    pkinfo.size = sbuf.size();

    write(sbuf, pkinfo);
}

void ClientImpl::write(const uint64_t& id,
                   const uint32_t& type)
{
    PacketInfo pkinfo;

    pkinfo.id = id;
    pkinfo.type = type;
    pkinfo.size = 0;

    write(pkinfo);
}

void ClientImpl::write(const PacketInfo& pktinfo)
{
    size_t nsent = boost::asio::write(socket_,
                                      boost::asio::buffer(reinterpret_cast<const uint8_t*>(&pktinfo),
                                                          sizeof(PacketInfo)));

    if(nsent != sizeof(PacketInfo))
        throw runtime_error("failed to send entire stream");
}

PacketInfo ClientImpl::read(std::vector<char>& buf)
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

PacketInfo ClientImpl::read(boost::asio::streambuf& sbuf)
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

PacketInfo ClientImpl::read()
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

PacketInfo ClientImpl::read(boost::posix_time::time_duration timeout)
{
    boost::system::error_code ec = boost::asio::error::would_block;
    PacketInfo pktinfo;

    deadline_timer_.expires_from_now(timeout);

    boost::asio::async_read(socket_,
                            boost::asio::buffer(reinterpret_cast<uint8_t*>(&pktinfo),
                                                sizeof(PacketInfo)),
                            bl::var(ec) = bl::_1);
    do
    {
        io_service_.run_one();

    } while(ec == boost::asio::error::would_block);

    if(ec)
      throw boost::system::system_error(ec); // Some other error.

    reset_deadline_timer();

    return pktinfo;
}

} // namespace crete
