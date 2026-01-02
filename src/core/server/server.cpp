#include <comms/tcp-server.hpp>
#include <comms/udp-server.hpp>
#include <server.hpp>

namespace sphys
{

Server::Server()
    : config("defs/server.yaml"), sendTimer(ioContext),
      signals(ioContext, SIGINT, SIGTERM)
{
    logging::createLogger("logs/logServer.txt");
    startServer();
}
Server::~Server() {}

void Server::startUdpTcp()
{
    int portTcp = static_cast<int>(std::get<float>(config.get({"port-tcp"})));
    int portUdp = static_cast<int>(std::get<float>(config.get({"port-udp"})));
    LG_D("Setup socket on port-tcp={} and port-udp={}", portTcp, portUdp);

    signals.async_wait(
        [&](const boost::system::error_code&, int)
        {
            spdlog::info("Signal received, shutting down...");
            ioContext.stop();  // Stop all IO operations
        });

    tcpServer = std::make_unique<TcpServer>(ioContext, portTcp);
    udpServer = std::make_unique<UdpServer>(ioContext, portUdp);

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
    sendTimer.expires_after(std::chrono::milliseconds(1000));
    sendTimer.async_wait(
        [this](boost::system::error_code ec)
        {
            if (!ec)
            {
                SendRequest sendRequest;
                while(engine.sendQueue.try_dequeue(sendRequest))
                {
                    udpServer->sendMessage(sendRequest.address, sendRequest.data);
                }
                scheduleSend();  // schedule next check
            } else {
                LG_E("Send timer aborted");
            }
        });
}


}  // namespace sphys