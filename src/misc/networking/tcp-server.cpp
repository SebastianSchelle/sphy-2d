#include "tcp-server.hpp"
#include <boost/system/detail/error_code.hpp>

namespace net
{

TcpConnection::pointer
TcpConnection::create(boost::asio::io_context& io_context,
                      ReceiveCallbackConn receiveCallback,
                      TcpDisconnectCallback disconnectCallback)
{
    return pointer(new TcpConnection(
        io_context, receiveCallback, std::move(disconnectCallback)));
}

tcp::socket& TcpConnection::socket()
{
    return socket_;
}

void TcpConnection::start()
{
    running.store(true);
    doRead();
}

void TcpConnection::close()
{
    auto self = shared_from_this();
    boost::asio::dispatch(socket_.get_executor(), [this, self]()
    {
        if (!running.exchange(false))
            return;
        LG_D("Closing TCP connection");
        boost::system::error_code ec;
        if(socket_.cancel(ec))
        {
            LG_W("Failed to cancel socket: {}", ec.message());
        }
        if(socket_.shutdown(tcp::socket::shutdown_both, ec))
        {
            LG_W("Failed to shutdown socket: {}", ec.message());
        }
        if(socket_.close(ec))
        {
            LG_W("Failed to close socket: {}", ec.message());
        }
        fireDisconnectOnce();
    });
}

TcpConnection::TcpConnection(boost::asio::io_context& io_context,
                             ReceiveCallbackConn receiveCallback,
                             TcpDisconnectCallback disconnectCallback)
    : socket_(io_context),
      receiveCallback(std::move(receiveCallback)),
      disconnectCallback(std::move(disconnectCallback)),
      clientInfoHandle(ClientInfoHandle::Invalid())
{
}

void TcpConnection::fireDisconnectOnce()
{
    if (disconnectNotified.exchange(true))
        return;
    if (disconnectCallback)
        disconnectCallback(shared_from_this());
}

void TcpConnection::doRead()
{
    auto self(shared_from_this());
    socket_.async_read_some(
        boost::asio::buffer(recvBuf, TCP_REC_BUF_LEN),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
            if (!ec && length > 0 && running.load())
            {
                if (receiveCallback)
                {
                    receiveCallback(recvBuf, length, self);
                }
                else
                {
                    LG_D("Received: {}", std::string(recvBuf, length));
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
    auto self(shared_from_this());
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
                     ReceiveCallbackConn receiveCallback,
                     TcpDisconnectCallback disconnectCallback)
    : io_context_(io_context),
      acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
      receiveCallback(std::move(receiveCallback)),
      disconnectCallback(std::move(disconnectCallback))
{
    LG_D("Starting TCP server on port {}", port);
    StartAccept();
}

void TcpServer::StartAccept()
{
    TcpConnection::pointer new_connection = TcpConnection::create(
        io_context_, receiveCallback, disconnectCallback);

    acceptor_.async_accept(
        new_connection->socket(),
        [this, new_connection](const boost::system::error_code& ec)
        { HandleAccept(new_connection, ec); });
}

void TcpServer::HandleAccept(TcpConnection::pointer new_connection,
                             const boost::system::error_code& error)
{
    if (!error)
    {
        new_connection->start();
    }

    StartAccept();
}

}  // namespace net