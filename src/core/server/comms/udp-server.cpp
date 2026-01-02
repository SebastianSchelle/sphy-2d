#include "udp-server.hpp"
#include <logging.hpp>

using boost::asio::ip::udp;

UdpServer::UdpServer(boost::asio::io_context& io_context, int port)
    : socket_(io_context, udp::endpoint(udp::v4(), port))
{
    StartReceive();
}

void UdpServer::StartReceive()
{
    socket_.async_receive_from(
        boost::asio::buffer(recv_buffer_),
        remote_endpoint_,
        [this](const boost::system::error_code& ec, std::size_t bytes)
        { HandleReceive(ec, bytes); });
}

void UdpServer::HandleReceive(const boost::system::error_code& error,
                                 size_t bytes_received)
{
    if (!error && bytes_received > 0)
    {
        std::shared_ptr<std::string> message(
            std::make_shared<std::string>("hello world"));

        socket_.async_send_to(
            boost::asio::buffer(*message),
            remote_endpoint_,
            std::bind(&UdpServer::handleSend, this, message));

        StartReceive();
    }
}

void UdpServer::handleSend(std::shared_ptr<std::string> message)
{
}

void UdpServer::sendMessage(address address, con::vector<uint8_t> data)
{
    udp::endpoint endpoint(address, 8088);
    socket_.send_to(boost::asio::buffer(data), endpoint);
}

