#include "udp-server.hpp"
#include <logging.hpp>


UdpServer::UdpServer(boost::asio::io_context& io_context, int port)
    : socket_(io_context, udp::endpoint(udp::v4(), port))
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
        // socket_.async_send_to(
        //     boost::asio::buffer(recvBuf, bytesReceived),
        //     remote_endpoint_,
        //     [this](const boost::system::error_code& ec, std::size_t bytes){
        //         if(ec)
        //         {
        //             LG_W("Send failed: {}", ec.message());
        //         }
        //     });
        startReceive();
    }
}

void UdpServer::sendMessage(asio::ip::address address, int port, std::vector<uint8_t> data)
{
    udp::endpoint endpoint(address, port);

    socket_.async_send_to(
        boost::asio::buffer(data),
        endpoint,
        [this](const boost::system::error_code& ec, std::size_t bytes){
            if(ec)
            {
                LG_W("Send failed: {}", ec.message());
            }
        });
}
