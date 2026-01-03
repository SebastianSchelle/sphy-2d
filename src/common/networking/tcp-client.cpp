#include "tcp-client.hpp"

namespace net
{

TcpClient::TcpClient(boost::asio::io_context& io_context,
                     tcp::endpoint endpoint,
                     net::ReceiveCallback receiveCallback)
    : socket(io_context), devServerEndpoint(endpoint), receiveCallback(receiveCallback)
{
    socket.connect(devServerEndpoint);
}

void TcpClient::sendMessage(const std::vector<uint8_t>& data)
{
    LG_D("Sending TCP message");
    socket.async_send(boost::asio::buffer(data),
                      [this](const boost::system::error_code& ec,
                             std::size_t bytes) {
                                if(ec)
                                {
                                    LG_W("TCP send failed: {}", ec.message());
                                }
                             });
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
        receiveCallback(recvBuf, bytes_received);
    }
}

}  // namespace net