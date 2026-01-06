#include "tcp-client.hpp"

namespace net
{

TcpClient::TcpClient(boost::asio::io_context& io_context,
                     tcp::endpoint endpoint,
                     net::ReceiveCallback receiveCallback)
    : socket(io_context), devServerEndpoint(endpoint), receiveCallback(receiveCallback)
{
    socket.connect(devServerEndpoint);
    startReceive();
}

void TcpClient::sendMessage(const std::vector<uint8_t>& data)
{
    LG_D("Sending TCP message");
    try{
        size_t bytesSent = socket.send(boost::asio::buffer(data));
    }
    catch(const std::exception& e)
    {
        LG_W("TCP send failed: {}", e.what());
    }
}

void TcpClient::startReceive()
{
    socket.async_receive(
        boost::asio::buffer(recvBuf, TCP_REC_BUF_LEN),
        [this](const boost::system::error_code& ec, std::size_t bytes)
        { handleReceive(ec, bytes); });
}

void TcpClient::handleReceive(const boost::system::error_code& error,
                              size_t bytes_received)
{
    if (!error && bytes_received > 0)
    {
        if(receiveCallback)
        {
            receiveCallback(recvBuf, bytes_received);
        }
        else
        {
            LG_D("Received: {}", std::string(recvBuf, bytes_received));
        }
        startReceive();
    }
    else
    {
        LG_E("TCP receive failed: {}", error.message());
    }
}

}  // namespace net