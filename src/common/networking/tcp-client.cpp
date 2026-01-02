#include "tcp-client.hpp"

TcpClient::TcpClient(boost::asio::io_context& io_context, int port)
    : socket(io_context)
{
    socket.open(tcp::v4());
}