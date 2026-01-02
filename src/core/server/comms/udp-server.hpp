#ifndef UDP_SERVER_HPP
#define UDP_SERVER_HPP

#include <boost/asio.hpp>
#include <std-inc.hpp>

using boost::asio::ip::udp;
using boost::asio::ip::address;

class UdpServer
{
  public:
    UdpServer(boost::asio::io_context& io_context, int port);
    void sendMessage(address address, con::vector<uint8_t> data);
  private:
    void StartReceive();

    void HandleReceive(const boost::system::error_code& error,
                        size_t bytes_received);

    void handleSend(std::shared_ptr<std::string> message);

    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    std::array<char, 1> recv_buffer_;
};

#endif