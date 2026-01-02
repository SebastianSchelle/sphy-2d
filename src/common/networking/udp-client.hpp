#ifndef UDP_CLIENT_HPP
#define UDP_CLIENT_HPP

#include <std-inc.hpp>
#include <net-shared.hpp>

static const size_t UDP_REC_BUF_LEN = 1024;

namespace net
{

class UdpClient
{
  public:
    UdpClient(boost::asio::io_context& io_context,
              int port,
              udp::endpoint endpoint,
              ReceiveCallback receiveCallback);

    void sendMessage(const std::vector<uint8_t>& data);

    void sendMessageTo(udp::endpoint endpoint, const con::vector<uint8_t> data);

    void startReceive();

    void handleReceive(const boost::system::error_code& error,
                       size_t bytes_received);

  private:
    udp::socket socket;
    udp::endpoint devServerEndpoint;
    char recvBuf[UDP_REC_BUF_LEN];
    ReceiveCallback receiveCallback;
};

}  // namespace net

#endif