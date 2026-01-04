#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <sphy-2d.hpp>
// #include <tcp-client.hpp>
#include <config-manager/config-manager.hpp>
#include <udp-client.hpp>
#include <tcp-client.hpp>
#include <model.hpp>
#include <client-def.hpp>
#include <protocol.hpp>

namespace sphyc
{

class Client
{
  public:
    Client(cfg::ConfigManager& config);
    ~Client();
    void startClient();
    void connectToServer();
    void shutdown();
    void wait();  // Wait for model thread to finish

  private:
    void scheduleSend();
    void udpReceive(const char* data, size_t length);
    void tcpReceive(const char* data, size_t length);

    Model model;
    std::unique_ptr<net::UdpClient> udpClient;
    std::unique_ptr<net::TcpClient> tcpClient;
    boost::asio::io_context ioContext;
    std::thread ioThread;
    boost::asio::signal_set signals;
    cfg::ConfigManager& config;
    boost::asio::steady_timer sendTimer;
    net::ClientInfo clientInfo;
    std::atomic<bool> shuttingDown{false};
    std::atomic<bool> spdlogShutdown{false};
};

}  // namespace sphys

#endif