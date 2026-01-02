#ifndef UDP_SERVER_HPP
#define UDP_SERVER_HPP

#include <std-inc.hpp>


static const size_t UDP_REC_BUF_LEN = 1024;

class UdpServer
{
  public:
    UdpServer(boost::asio::io_context& io_context, int port);
    void sendMessage(asio::ip::address address, int port, std::vector<uint8_t> data);
  private:
    void startReceive();

    void handleReceive(const boost::system::error_code& error,
                        size_t bytes_received);

    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    char recvBuf[UDP_REC_BUF_LEN];
};

#endif