#include <server.hpp>

namespace sphys
{

Server::Server(def::CmdLinOptionsServer& options)
    : options(options),
      config(options.workingdir + "/modules/core/defs/server.yaml"),
      sendTimer(ioContext), signals(ioContext, SIGINT, SIGTERM)
{
    auto path(options.workingdir);
    std::filesystem::current_path(path);
    uint8_t logLevel =
        static_cast<uint8_t>(std::get<float>(config.get({"loglevel"})));
    debug::createLogger("logs/logServer.txt", logLevel);
    startServer();
}
Server::~Server() {}

void Server::startUdpTcp()
{
    int portTcp = static_cast<int>(
        std::get<float>(config.get({"connection", "serv-port-tcp"})));
    int portUdp = static_cast<int>(
        std::get<float>(config.get({"connection", "serv-port-udp"})));
    LG_D("Setup socket on port-tcp={} and port-udp={}", portTcp, portUdp);

    signals.async_wait(
        [&](const boost::system::error_code&, int)
        {
            LG_I("Signal received, shutting down...");
            ioContext.stop();  // Stop all IO operations
        });

    tcpServer =
        std::make_unique<net::TcpServer>(ioContext,
                                         portTcp,
                                         std::bind(&Server::tcpReceive,
                                                   this,
                                                   std::placeholders::_1,
                                                   std::placeholders::_2,
                                                   std::placeholders::_3));
    LG_D("Setup socket on port-tcp={}", portTcp);

    udpServer =
        std::make_unique<net::UdpServer>(ioContext,
                                         portUdp,
                                         std::bind(&Server::udpReceive,
                                                   this,
                                                   std::placeholders::_1,
                                                   std::placeholders::_2));
    LG_D("Setup socket on port-udp={}", portUdp);

    ioThread = std::thread([this]() { ioContext.run(); });
    scheduleSend();
}

void Server::startServer()
{
    startUdpTcp();
    startEngine();
    ioThread.join();
    spdlog::shutdown();
}

void Server::startEngine()
{
    engine.start();
}

void Server::scheduleSend()
{
    sendTimer.expires_after(std::chrono::milliseconds(10));
    sendTimer.async_wait(
        [this](boost::system::error_code ec)
        {
            if (!ec)
            {
                net::CmdQueueData sendRequest;
                while (engine.sendQueue.try_dequeue(sendRequest))
                {
                    if (sendRequest.sendType == net::SendType::UDP)
                    {
                        udpServer->sendMessage(sendRequest.udpEndpoint,
                                               sendRequest.data);
                    }
                    else if (sendRequest.sendType == net::SendType::TCP)
                    {
                        sendRequest.tcpConnection->sendMessage(
                            sendRequest.data);
                    }
                }
                scheduleSend();  // schedule next check
            }
            else
            {
                LG_E("Send timer aborted");
            }
        });
}


void Server::udpReceive(const char* data, size_t length)
{
    if (length >= 21)
    {
        net::CmdQueueData cmdData;
        cmdData.sendType = net::SendType::UDP;
        cmdData.data.insert(cmdData.data.end(), data, data + length);
        engine.receiveQueue.enqueue(cmdData);
    }
}

void Server::tcpReceive(const char* data,
                        size_t length,
                        std::shared_ptr<net::TcpConnection> connection)
{
    if (length >= 5)
    {
        net::CmdQueueData cmdData;
        cmdData.sendType = net::SendType::TCP;
        cmdData.tcpConnection = connection;
        cmdData.data.insert(cmdData.data.end(), data, data + length);
        engine.receiveQueue.enqueue(cmdData);
    }
}


}  // namespace sphys