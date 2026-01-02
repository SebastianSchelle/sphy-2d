#ifndef UDP_SERVER_HPP
#define UDP_SERVER_HPP

#include <net-shared.hpp>
#include <std-inc.hpp>

namespace net
{

static const size_t UDP_REC_BUF_LEN = 1024;

class UdpServer
{
  public:
    UdpServer(boost::asio::io_context& io_context,
              int port,
              ReceiveCallback receiveCallback);
    void sendMessage(asio::ip::address address,
                     int port,
                     const std::vector<uint8_t>& data);

  private:
    void startReceive();

    void handleReceive(const boost::system::error_code& error,
                       size_t bytes_received);

    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    char recvBuf[UDP_REC_BUF_LEN];
    ReceiveCallback receiveCallback;
};

}  // namespace net

#endif