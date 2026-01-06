#include "tcp-server.hpp"

namespace net
{

TcpConnection::pointer
TcpConnection::create(boost::asio::io_context& io_context,
                      ReceiveCallbackConn receiveCallback)
{
    return pointer(new TcpConnection(io_context, receiveCallback));
}

tcp::socket& TcpConnection::socket()
{
    return socket_;
}

void TcpConnection::start()
{
    running = true;
    doRead();
}

void TcpConnection::close()
{
    LG_D("Closing TCP connection");
    running = false;
    socket_.shutdown(tcp::socket::shutdown_both);
}

TcpConnection::TcpConnection(boost::asio::io_context& io_context,
                             ReceiveCallbackConn receiveCallback)
    : socket_(io_context), receiveCallback(receiveCallback)
{
}

void TcpConnection::doRead()
{
    auto self(shared_from_this());
    socket_.async_read_some(
        boost::asio::buffer(recvBuf, TCP_REC_BUF_LEN),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
            if (!ec && length > 0 && running)
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
    try{
        size_t bytesSent = socket_.send(boost::asio::buffer(data));
    }
    catch(const std::exception& e)
    {
        LG_W("TCP send failed: {}", e.what());
        close();
    }
}

TcpServer::TcpServer(boost::asio::io_context& io_context,
                     int port,
                     ReceiveCallbackConn receiveCallback)
    : io_context_(io_context),
      acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
      receiveCallback(receiveCallback)
{
    LG_D("Starting TCP server on port {}", port);
    StartAccept();
}

void TcpServer::StartAccept()
{
    TcpConnection::pointer new_connection =
        TcpConnection::create(io_context_, receiveCallback);

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