#include <crete/asio/client.h>

#include "client_pimpl.h"

#include <vector>

using namespace std;

namespace crete
{

Client::Client(const std::string& host_ip, const std::string& port) :
    pimpl_(new ClientImpl(host_ip, port))
{
}

Client::~Client()
{
    if(pimpl_)
        delete pimpl_;
}

void Client::connect()
{
    pimpl_->connect();
}

void Client::connect(boost::posix_time::time_duration timeout)
{
    pimpl_->connect(timeout);
}

void Client::write_message(const std::string& msg)
{
    pimpl_->write_message(msg);
}

string Client::read_message()
{
    return pimpl_->read_message();
}

size_t Client::write(const std::vector<char>& buf, const PacketInfo& pkinfo)
{
    return pimpl_->write(buf, pkinfo);
}

void Client::write(boost::asio::streambuf& sbuf, const PacketInfo& pktinfo)
{
    pimpl_->write(sbuf, pktinfo);
}

void Client::write(boost::asio::streambuf& sbuf, const uint64_t& id, const uint32_t& type)
{
    pimpl_->write(sbuf, id, type);
}

void Client::write(const uint64_t& id, const uint32_t& type)
{
    pimpl_->write(id, type);
}

void Client::write(const PacketInfo& pktinfo)
{
    pimpl_->write(pktinfo);
}

PacketInfo Client::read(std::vector<char>& buf)
{
    return pimpl_->read(buf);
}

PacketInfo Client::read(boost::asio::streambuf& sbuf)
{
    return pimpl_->read(sbuf);
}

PacketInfo Client::read()
{
    return pimpl_->read();
}

PacketInfo Client::read(boost::posix_time::time_duration timeout)
{
    return pimpl_->read(timeout);
}

} // namespace crete
