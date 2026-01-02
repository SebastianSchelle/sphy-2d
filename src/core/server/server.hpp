#ifndef SERVER_HPP
#define SERVER_HPP

#include <boost/asio.hpp>
#include <boost/asio/ip/address.hpp>
#include <tcp-server.hpp>
#include <udp-server.hpp>
#include <config-manager/config-manager.hpp>
#include <engine.hpp>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using udp = asio::ip::udp;

namespace sphys
{

class Server
{
  public:
    Server();
    ~Server();
    void startUdpTcp();
    void startServer();
    void startEngine();

  private:
    void scheduleSend();

    sphys::Engine engine;
    cfg::ConfigManager config;
    boost::asio::io_context ioContext;
    std::thread ioThread;
    asio::steady_timer sendTimer;
    std::unique_ptr<UdpServer> udpServer;
    std::unique_ptr<TcpServer> tcpServer;
    boost::asio::signal_set signals;
};

}  // namespace sphys

#endif