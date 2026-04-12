#include "tcp-client.hpp"

namespace net
{

TcpClient::TcpClient(boost::asio::io_context& io_context,
                     tcp::endpoint endpoint,
                     TcpReceiveClb tcpReceiveCallback,
                     ConnectionClosedCallback connectionClosedCallback)
    : socket(io_context), devServerEndpoint(endpoint),
      tcpReceiveCallback(tcpReceiveCallback),
      connectionClosedCallback(connectionClosedCallback)
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
    try
    {
        size_t bytesSent = socket.send(boost::asio::buffer(data));
    }
    catch (const std::exception& e)
    {
        LG_W("TCP send failed: {}", e.what());
    }
}

void TcpClient::startReceive()
{
    rcvdCmd.sendType = SendType::TCP;
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
        for (size_t i = 0; i < bytes_received; i++)
        {
            rcvdCmd.data.push_back(recvBuf[i]);
            switch (rcvCmdState)
            {
                case RcvCmdState::ParseCmd0:
                    rcvCmdState = RcvCmdState::ParseCmd1;
                    break;
                case RcvCmdState::ParseCmd1:
                    rcvCmdState = RcvCmdState::ParseFlags;
                    break;
                case RcvCmdState::ParseFlags:
                    rcvCmdState = RcvCmdState::ParseLen0;
                    break;
                case RcvCmdState::ParseLen0:
                    rcvCmdLen = recvBuf[i];
                    rcvCmdState = RcvCmdState::ParseLen1;
                    break;
                case RcvCmdState::ParseLen1:
                    rcvCmdLen = rcvCmdLen | (recvBuf[i] << 8);
                    lastDataStart = rcvdCmd.data.size();
                    rcvCmdState = rcvCmdLen ? RcvCmdState::ParseData
                                            : RcvCmdState::ParseCmd0;
                    break;
                case RcvCmdState::ParseData:
                    if (rcvdCmd.data.size() - lastDataStart == rcvCmdLen)
                    {
                        rcvCmdState = RcvCmdState::ParseCmd0;
                        // maybe flush at great vector size?
                    }
                    break;
            }
        }
        if (rcvCmdState == RcvCmdState::ParseCmd0)
        {
            LG_D("TCP packet received: {}", rcvdCmd.data.size());
            // Packet is complete at stream boundary
            if (tcpReceiveCallback)
            {
                tcpReceiveCallback(rcvdCmd);
            }
            rcvdCmd.data.clear();
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

        if (connectionClosedCallback)
        {
            connectionClosedCallback();
        }
        else
        {
            LG_W("No connection closed callback set");
        }
    }
}

}  // namespace net