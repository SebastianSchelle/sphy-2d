#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include <std-inc.hpp>

namespace net
{

static const size_t TCP_REC_BUF_LEN = 1024;

class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
  public:
    typedef std::shared_ptr<TcpConnection> pointer;

    static pointer create(boost::asio::io_context& io_context);

    tcp::socket& socket();

    void start();

  private:
    TcpConnection(boost::asio::io_context& io_context);

    void doRead();
    void doWrite(const std::string& msg);
    void handle_write();

    tcp::socket socket_;
    char recvBuf[TCP_REC_BUF_LEN];
};

class TcpServer
{
  public:
    TcpServer(boost::asio::io_context& io_context, int port);

  private:
    void StartAccept();

    void HandleAccept(TcpConnection::pointer new_connection,
                       const boost::system::error_code& error);


    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
};

}  // namespace net

#endif  // TCP_SERVER_HPP