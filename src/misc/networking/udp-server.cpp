#include "udp-server.hpp"
#include <logging.hpp>

namespace net
{

UdpServer::UdpServer(boost::asio::io_context& io_context, int port, ReceiveCallback receiveCallback)
    : socket_(io_context, udp::endpoint(udp::v4(), port)),
      receiveCallback(receiveCallback)
{
    startReceive();
}

void UdpServer::startReceive()
{
    socket_.async_receive_from(
        boost::asio::buffer(recvBuf),
        remote_endpoint_,
        [this](const boost::system::error_code& ec, std::size_t bytes)
        { handleReceive(ec, bytes); });
}

void UdpServer::handleReceive(const boost::system::error_code& error,
                              size_t bytesReceived)
{
    if (!error && bytesReceived > 0)
    {
        if(receiveCallback)
        {
            receiveCallback(remote_endpoint_, recvBuf, bytesReceived);
        }
        else
        {
            LG_D("Received: {}", std::string(recvBuf, bytesReceived));
        }
    }
    startReceive();
}

void UdpServer::sendMessage(udp::endpoint endpoint,
                            const std::vector<uint8_t>& data)
{
    socket_.async_send_to(
        boost::asio::buffer(data),
        endpoint,
        [this, endpoint](const boost::system::error_code& ec, std::size_t bytes)
        {
            if (ec)
            {
                LG_W("Send failed: {}", ec.message());
            }
        });
}

}  // namespace net