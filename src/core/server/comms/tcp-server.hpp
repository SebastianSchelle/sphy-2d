#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include <boost/asio.hpp>
#include <std-inc.hpp>
#include <logging.hpp>

using boost::asio::ip::tcp;


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
    enum { max_length = 1024 };
    char data_[max_length];
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

#endif  // TCP_SERVER_HPP