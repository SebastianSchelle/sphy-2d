#ifndef CLIENT_HPP
#define CLIENT_HPP

// #include <tcp-client.hpp>
#include <client-def.hpp>
#include <config-manager/config-manager.hpp>
#include <protocol.hpp>
#include <tcp-client.hpp>
#include <udp-client.hpp>
#include <concurrentqueue.h>
#include <mutex>

using moodycamel::ConcurrentQueue;

namespace sphyc
{

class Client
{
  public:
    Client(cfg::ConfigManager& config,
           ConcurrentQueue<net::CmdQueueData>& modelSendQueue,
           ConcurrentQueue<net::CmdQueueData>& modelReceiveQueue);
    ~Client();
    void connectToServer(const std::string& ipAddress,
                         int udpPortServ,
                         int tcpPortServ,
                         int udpPortCli,
                         const std::string& token);
    void setShutdownCallback(std::function<void()> cb)
    {
        shutdownCallback = std::move(cb);
    }
    void shutdown();
    void wait();  // Wait for model thread to finish

  private:
    void scheduleSend(const std::string& token);
    void udpReceive(const char* data, size_t length);
    void tcpReceive(const char* data, size_t length);

    std::unique_ptr<net::UdpClient> udpClient;
    std::unique_ptr<net::TcpClient> tcpClient;
    boost::asio::io_context ioContext;
    std::thread ioThread;
    cfg::ConfigManager& config;
    boost::asio::steady_timer sendTimer;
    std::atomic<bool> shuttingDown{false};
    std::atomic<bool> spdlogShutdown{false};
    std::atomic<bool> shutdownNotified{false};
    std::function<void()> shutdownCallback;
    std::mutex lifecycleMutex;
    ConcurrentQueue<net::CmdQueueData>& modelSendQueue;
    ConcurrentQueue<net::CmdQueueData>& modelReceiveQueue;
};

}  // namespace sphyc

#endif