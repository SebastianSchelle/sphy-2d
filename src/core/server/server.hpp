#ifndef SERVER_HPP
#define SERVER_HPP

#include <boost/asio.hpp>
#include <boost/asio/ip/address.hpp>
#include <tcp-server.hpp>
#include <udp-server.hpp>
#include <config-manager/config-manager.hpp>
#include <engine.hpp>
#include <cmd-options.hpp>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using udp = asio::ip::udp;

namespace sphys
{

class Server
{
  public:
    Server(sphy::CmdLinOptionsServer& options);
    ~Server();
    void startUdpTcp();
    void startServer();
    void startEngine();

  private:
    void scheduleSend();
    void udpReceive(const char* data, size_t length);
    void tcpReceive(const char* data, size_t length, std::shared_ptr<net::TcpConnection> connection);

    sphys::Engine engine;
    cfg::ConfigManager config;
    boost::asio::io_context ioContext;
    std::thread ioThread;
    asio::steady_timer sendTimer;
    std::unique_ptr<net::UdpServer> udpServer;
    std::unique_ptr<net::TcpServer> tcpServer;
    boost::asio::signal_set signals;
    sphy::CmdLinOptionsServer options;
};

}  // namespace sphys

#endif