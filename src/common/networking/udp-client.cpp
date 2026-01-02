#include "udp-client.hpp"


UdpClient::UdpClient(boost::asio::io_context& io_context,
                     int port,
                     udp::endpoint endpoint,
                     ReceiveCallback receiveCallback)
    : socket(io_context), devServerEndpoint(endpoint),
      receiveCallback(receiveCallback)
{
    socket.open(udp::v4());
    socket.bind(udp::endpoint(udp::v4(), port));
    startReceive();
}

void UdpClient::sendMessage(const con::vector<uint8_t> data)
{
    socket.send_to(boost::asio::buffer(data), devServerEndpoint);
}

void UdpClient::sendMessageTo(udp::endpoint endpoint,
                               const con::vector<uint8_t> data)
{
    socket.send_to(boost::asio::buffer(data), endpoint);
}

void UdpClient::startReceive()
{
    LG_D("Starting receive from {}", devServerEndpoint.address().to_string());
    socket.async_receive_from(
        boost::asio::buffer(recvBuf, UDP_REC_BUF_LEN),
        devServerEndpoint,
        [this](const boost::system::error_code& ec, std::size_t bytes)
        { handleReceive(ec, bytes); });
}

void UdpClient::handleReceive(const boost::system::error_code& error,
                              size_t bytes_received)
{
    if (!error && bytes_received > 0)
    {
        if (receiveCallback)
        {
            receiveCallback(recvBuf, bytes_received);
        }
        else
        {
            LG_D("Received: {}", std::string(recvBuf, bytes_received));
        }
    }
    startReceive();
}

