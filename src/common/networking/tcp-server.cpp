#include "tcp-server.hpp"

TcpConnection::pointer
TcpConnection::create(boost::asio::io_context& io_context)
{
    return pointer(new TcpConnection(io_context));
}

tcp::socket& TcpConnection::socket()
{
    return socket_;
}

void TcpConnection::start()
{
    doRead();
}

TcpConnection::TcpConnection(boost::asio::io_context& io_context)
    : socket_(io_context)
{
}

void TcpConnection::doRead()
{
    auto self(shared_from_this());
    socket_.async_read_some(
        boost::asio::buffer(recvBuf, TCP_REC_BUF_LEN),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
            if (!ec)
            {
                std::string received(recvBuf, length);
                LG_D("Received: {}", received);

                // Echo or process
                doWrite("Echo: " + received);

                // Continue reading
                doRead();
            }
            else
            {
                LG_W("Connection closed: {}", ec.message());
            }
        });
}

void TcpConnection::doWrite(const std::string& msg)
{
    auto self(shared_from_this());
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(msg),
        [this, self](boost::system::error_code ec, std::size_t)
        {
            if (ec)
            {
                LG_W("Write failed: {}", ec.message());
            }
        });
}

TcpServer::TcpServer(boost::asio::io_context& io_context, int port)
    : io_context_(io_context),
      acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
{
    LG_D("Starting TCP server on port {}", port);
    StartAccept();
}

void TcpServer::StartAccept()
{
    TcpConnection::pointer new_connection =
        TcpConnection::create(io_context_);

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
