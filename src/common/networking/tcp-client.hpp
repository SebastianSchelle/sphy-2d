#ifndef TCP_CLIENT_HPP
#define TCP_CLIENT_HPP

#include <std-inc.hpp>

static const size_t TCP_REC_BUF_LEN = 1024;

class TcpClient
{
  public:
    TcpClient(boost::asio::io_context& io_context, int port);

  private:
    tcp::socket socket;
    char recvBuf[TCP_REC_BUF_LEN];
};

#endif