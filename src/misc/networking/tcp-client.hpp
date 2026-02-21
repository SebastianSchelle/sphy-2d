#ifndef TCP_CLIENT_HPP
#define TCP_CLIENT_HPP

#include <net-shared.hpp>
#include <std-inc.hpp>

namespace net
{

static const size_t TCP_REC_BUF_LEN = 1024;

class TcpClient
{
  public:
    TcpClient(boost::asio::io_context& io_context,
              tcp::endpoint endpoint,
              net::TcpReceiveCallback receiveCallback);
    void sendMessage(const std::vector<uint8_t>& data);
    void startReceive();
    void handleReceive(const boost::system::error_code& error,
                       size_t bytes_received);

  private:
    tcp::socket socket;
    char recvBuf[TCP_REC_BUF_LEN];
    net::TcpReceiveCallback receiveCallback;
    tcp::endpoint devServerEndpoint;
};

}  // namespace net

#endif