#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include <net-shared.hpp>
#include <std-inc.hpp>

namespace net
{
static const size_t TCP_REC_BUF_LEN = 1024;

class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
  public:
    typedef std::shared_ptr<TcpConnection> pointer;

    static pointer create(boost::asio::io_context& io_context,
                          ReceiveCallbackConn receiveCallback);

    tcp::socket& socket();
    
    void sendMessage(const std::vector<uint8_t>& data);
    void start();
    void close();

  private:
    TcpConnection(boost::asio::io_context& io_context,
                  ReceiveCallbackConn receiveCallback);

    void doRead();
    void handle_write();

    tcp::socket socket_;
    bool running;
    char recvBuf[TCP_REC_BUF_LEN];
    ReceiveCallbackConn receiveCallback;
};

class TcpServer
{
  public:
    TcpServer(boost::asio::io_context& io_context,
              int port,
              ReceiveCallbackConn receiveCallback);

  private:
    void StartAccept();

    void HandleAccept(TcpConnection::pointer new_connection,
                      const boost::system::error_code& error);


    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    ReceiveCallbackConn receiveCallback;
};

}  // namespace net

#endif  // TCP_SERVER_HPP