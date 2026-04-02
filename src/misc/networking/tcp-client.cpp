#include "tcp-client.hpp"

namespace net
{

TcpClient::TcpClient(boost::asio::io_context& io_context,
                     tcp::endpoint endpoint,
                     net::TcpReceiveCallback receiveCallback)
    : socket(io_context), devServerEndpoint(endpoint), receiveCallback(receiveCallback)
{
    socket.connect(devServerEndpoint);
    startReceive();
}

void TcpClient::close()
{
    boost::system::error_code ec;
    [[maybe_unused]] const auto cancelled = socket.cancel(ec);
    [[maybe_unused]] const auto shut =
        socket.shutdown(tcp::socket::shutdown_both, ec);
    [[maybe_unused]] const auto closed = socket.close(ec);
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
        if (error == boost::asio::error::eof)
        {
            LG_W("TCP connection closed by peer");
        }
        else
        {
            LG_E("TCP receive failed: {}", error.message());
        }

        if (receiveCallback)
        {
            // Signal connection close/drop to upper layer.
            receiveCallback(nullptr, 0);
        }
    }
}

}  // namespace net