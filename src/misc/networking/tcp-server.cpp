#include "tcp-server.hpp"
#include <boost/system/detail/error_code.hpp>

namespace net
{

std::unique_ptr<TcpConnection>
TcpConnection::create(boost::asio::io_context& io_context,
                      TcpReceiveClb receiveCallback,
                      TcpDisconnectCallback disconnectCallback)
{
    return std::unique_ptr<TcpConnection>(
        new TcpConnection(io_context, receiveCallback, disconnectCallback));
}

tcp::socket& TcpConnection::socket()
{
    return socket_;
}

void TcpConnection::start()
{
    running.store(true);
    rcvdCmd.sendType = SendType::TCP;
    rcvdCmd.tcpConnection = this;
    doRead();
}

void TcpConnection::close()
{
    boost::asio::dispatch(
        socket_.get_executor(),
        [this]()
        {
            if (!running.exchange(false))
                return;
            LG_D("Closing TCP connection");
            boost::system::error_code ec;
            if (socket_.cancel(ec))
            {
                LG_W("Failed to cancel socket: {}", ec.message());
            }
            if (socket_.shutdown(tcp::socket::shutdown_both, ec))
            {
                LG_W("Failed to shutdown socket: {}", ec.message());
            }
            if (socket_.close(ec))
            {
                LG_W("Failed to close socket: {}", ec.message());
            }
            fireDisconnectOnce();
        });
}

TcpConnection::TcpConnection(boost::asio::io_context& io_context,
                             TcpReceiveClb tcpReceiveCallback,
                             TcpDisconnectCallback disconnectCallback)
    : socket_(io_context), tcpReceiveCallback(std::move(tcpReceiveCallback)),
      disconnectCallback(std::move(disconnectCallback)),
      clientInfoHandle(TcpClientInfoHandle{0, 0})
{
}

void TcpConnection::fireDisconnectOnce()
{
    if (disconnectNotified.exchange(true))
        return;
    if (disconnectCallback)
        disconnectCallback(this);
}

void TcpConnection::doRead()
{
    socket_.async_read_some(
        boost::asio::buffer(recvBuf, TCP_REC_BUF_LEN),
        [this](boost::system::error_code ec, std::size_t length)
        {
            if (!ec && length > 0 && running.load())
            {
                for (size_t i = 0; i < length; i++)
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
                            rcvCmdState = rcvCmdLen ? RcvCmdState::ParseData : RcvCmdState::ParseCmd0;
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
                    // Packet is complete at stream boundary
                    if (tcpReceiveCallback)
                    {
                        tcpReceiveCallback(rcvdCmd);
                    }
                    rcvdCmd.data.clear();
                }
                doRead();
            }
            else
            {
                LG_W("Connection closed: {}", ec.message());
                close();
            }
        });
}

void TcpConnection::sendMessage(const std::vector<uint8_t>& data)
{
    if (!socket_.is_open())
    {
        LG_W("Tcp Socket not open");
        close();
        return;
    }
    try
    {
        size_t bytesSent = socket_.send(boost::asio::buffer(data));
    }
    catch (const std::exception& e)
    {
        LG_W("TCP send failed: {}", e.what());
        close();
    }
}

TcpServer::TcpServer(boost::asio::io_context& io_context,
                     int port,
                     TcpReceiveClb tcpReceiveCallback,
                     TcpDisconnectCallback disconnectCallback)
    : io_context_(io_context),
      acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
      tcpReceiveCallback(std::move(tcpReceiveCallback)),
      disconnectCallback(std::move(disconnectCallback))
{
    LG_D("Starting TCP server on port {}", port);
    StartAccept();
}

TcpServer::~TcpServer()
{
    close();
}

void TcpServer::close()
{
    boost::asio::dispatch(
        io_context_,
        [this]()
        {
            if (!running.exchange(false))
            {
                return;
            }
            boost::system::error_code ec;
            (void)acceptor_.cancel(ec);
            (void)acceptor_.close(ec);
            for (auto& conn : connections)
            {
                if (conn)
                {
                    conn->close();
                }
            }
        });
}

void TcpServer::StartAccept()
{
    if (!running.load())
    {
        return;
    }
    connections.emplace_back(
        TcpConnection::create(io_context_, tcpReceiveCallback, disconnectCallback));
    TcpConnection* new_connection = connections.back().get();

    acceptor_.async_accept(
        new_connection->socket(),
        [this, new_connection](const boost::system::error_code& ec)
        { HandleAccept(new_connection, ec); });
}

void TcpServer::HandleAccept(TcpConnection* new_connection,
                             const boost::system::error_code& error)
{
    if (!running.load())
    {
        return;
    }
    if (!error)
    {
        new_connection->start();
    }
    else if (error == boost::asio::error::operation_aborted)
    {
        return;
    }
    StartAccept();
}

}  // namespace net