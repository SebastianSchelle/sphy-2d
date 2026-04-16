#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include <atomic>
#include <std-inc.hpp>
#include <net-shared.hpp>

namespace net
{
static const size_t TCP_REC_BUF_LEN = 1024;

class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
  public:
    typedef std::shared_ptr<TcpConnection> pointer;

    static pointer create(boost::asio::io_context& io_context,
                          TcpReceiveClb tcpReceiveCallback,
                          TcpDisconnectCallback disconnectCallback);

    tcp::socket& socket();
    
    void sendMessage(const std::vector<uint8_t>& data);
    void start();
    void close();
    ClientInfoHandle getClientInfoHandle() const { return clientInfoHandle; }
    void setClientInfoHandle(ClientInfoHandle clientInfoHandle) { this->clientInfoHandle = clientInfoHandle; }

  private:
    TcpConnection(boost::asio::io_context& io_context,
                  TcpReceiveClb tcpReceiveCallback,
                  TcpDisconnectCallback disconnectCallback);

    void doRead();
    void handle_write();
    void fireDisconnectOnce();

    tcp::socket socket_;
    std::atomic<bool> running{false};
    uint8_t recvBuf[TCP_REC_BUF_LEN];
    ReceiveCallbackConn receiveCallback;
    TcpReceiveClb tcpReceiveCallback;
    TcpDisconnectCallback disconnectCallback;
    std::atomic<bool> disconnectNotified{false};
    ClientInfoHandle clientInfoHandle;
    RcvCmdState rcvCmdState = RcvCmdState::ParseCmd0;
    uint16_t rcvCmdLen = 0;
    uint16_t lastDataStart = 0;
    net::CmdQueueData rcvdCmd;
};

class TcpServer
{
  public:
    TcpServer(boost::asio::io_context& io_context,
              int port,
              TcpReceiveClb tcpReceiveCallback,
              TcpDisconnectCallback disconnectCallback);

  private:
    void StartAccept();

    void HandleAccept(TcpConnection::pointer new_connection,
                      const boost::system::error_code& error);


    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    TcpReceiveClb tcpReceiveCallback;
    TcpDisconnectCallback disconnectCallback;
};

}  // namespace net

#endif  // TCP_SERVER_HPP