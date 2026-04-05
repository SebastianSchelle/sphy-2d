#include <server.hpp>
#include <chrono>
#include <thread>

namespace sphys
{

Server::Server(sphy::CmdLinOptionsServer& options)
    : options(options),
      config(options.workingdir + "/modules/core/defs/server.yaml"),
      sendTimer(ioContext), signals(ioContext, SIGINT, SIGTERM),
      engine(options, config)
{
    auto path(options.workingdir);
    std::filesystem::current_path(path);
    uint8_t logLevel =
        static_cast<uint8_t>(std::get<float>(config.get({"loglevel"})));
    debug::createLogger("logs/logServer.txt", logLevel);
    // NOTE: Do not call startServer() here.
    // The base constructor runs before derived classes are fully constructed,
    // so virtual dispatch (e.g. registerSystems override) would call the base
    // version instead of the derived override.
}

Server::~Server() {
}

void Server::startUdpTcp()
{
    uint portTcp = CFG_UINT(config, 29200.0f, "connection", "serv-port-tcp");
    uint portUdp = CFG_UINT(config, 29201.0f, "connection", "serv-port-udp");
    LG_D("Setup socket on port-tcp={} and port-udp={}", portTcp, portUdp);

    signals.async_wait(
        [this](const boost::system::error_code& ec, int sig)
        {
            if (ec)
                return;
            LG_I("Signal received ({}), shutting down...", sig);
            engine.stop();  // save game and join engine thread
            ioContext.stop();
        });

    tcpServer = std::make_unique<net::TcpServer>(
        ioContext,
        portTcp,
        std::bind(&Server::tcpReceive,
                  this,
                  std::placeholders::_1,
                  std::placeholders::_2,
                  std::placeholders::_3),
        std::bind(&Server::tcpDisconnected,
                  this,
                  std::placeholders::_1));
    LG_D("Setup socket on port-tcp={}", portTcp);

    udpServer =
        std::make_unique<net::UdpServer>(ioContext,
                                         portUdp,
                                         std::bind(&Server::udpReceive,
                                                   this,
                                                   std::placeholders::_1,
                                                   std::placeholders::_2,
                                                   std::placeholders::_3));
    LG_D("Setup socket on port-udp={}", portUdp);

    ioContext.post([this]() { scheduleSend(); });
    ioThread = std::thread([this]() { ioContext.run(); });
}

void Server::startServer()
{
    startUdpTcp();
    startEngine();

    // Wait until engine has stopped (e.g. signal handler called engine.stop())
    while (!engine.stopped())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ioContext.stop();
    if (ioThread.joinable())
    {
        ioThread.join();
    }
    spdlog::shutdown();
}

void Server::startEngine()
{
    registerSystems(engine.ecs);
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

void Server::registerSystems(ecs::Ecs& ecs) {
}

void Server::udpReceive(udp::endpoint endpoint, const char* data, size_t length)
{
    if (length >= 21)
    {
        net::CmdQueueData cmdData;
        cmdData.sendType = net::SendType::UDP;
        cmdData.udpEndpoint = endpoint;
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

void Server::tcpDisconnected(std::shared_ptr<net::TcpConnection> connection)
{
    net::CmdQueueData cmdData;
    cmdData.sendType = net::SendType::TCP;
    cmdData.tcpConnection = std::move(connection);
    cmdData.tcpDisconnected = true;
    engine.receiveQueue.enqueue(cmdData);
}


}  // namespace sphys