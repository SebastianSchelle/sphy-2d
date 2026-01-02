#include <client.hpp>
#include <iostream>

namespace sphyc
{

Client::Client()
    : model(), config("defs/client.yaml"), sendTimer(ioContext),
      signals(ioContext, SIGINT, SIGTERM)
{
    uint8_t logLevel =
        static_cast<uint8_t>(std::get<float>(config.get({"loglevel"})));
    logging::createLogger("logs/logClient.txt", logLevel);
    startClient();
}
Client::~Client() {}

void Client::startClient()
{
    startUdpTcp();
    model.start();
    ioThread.join();
    spdlog::shutdown();
}

void Client::startUdpTcp()
{
    int portUdp = static_cast<int>(
        std::get<float>(config.get({"connection", "client-port-udp"})));
    int portTcp = static_cast<int>(
        std::get<float>(config.get({"connection", "client-port-tcp"})));
    int servPortUdp = static_cast<int>(
        std::get<float>(config.get({"connection", "serv-port-udp"})));
    int servPortTcp = static_cast<int>(
        std::get<float>(config.get({"connection", "serv-port-tcp"})));
    string serverIp =
        std::get<std::string>(config.get({"connection", "server-ip"}));

    signals.async_wait(
        [&](const boost::system::error_code&, int)
        {
            LG_I("Signal received, shutting down...");
            ioContext.stop();  // Stop all IO operations
        });

    // tcpClient = std::make_unique<TcpClient>(ioContext, portTcp);
    udpClient = std::make_unique<UdpClient>(
        ioContext,
        portUdp,
        udp::endpoint(boost::asio::ip::address::from_string(serverIp),
                      servPortUdp),
        std::bind(&Client::udpReceive,
                  this,
                  std::placeholders::_1,
                  std::placeholders::_2));
    LG_D("Setup udp socket to server at {}:{} on port {}",
         serverIp,
         servPortUdp,
         portUdp);

    ioThread = std::thread([this]() { ioContext.run(); });
    scheduleSend();
}

void Client::scheduleSend()
{
    sendTimer.expires_after(std::chrono::milliseconds(10));
    sendTimer.async_wait(
        [this](boost::system::error_code ec)
        {
            /*if (!ec)
            {
                SendRequest sendRequest;
                // while (engine.sendQueue.try_dequeue(sendRequest))
                {
                    sendRequest.data.push_back('H');
                    sendRequest.data.push_back('e');
                    sendRequest.data.push_back('l');
                    sendRequest.data.push_back('l');
                    sendRequest.data.push_back('o');
                    sendRequest.data.push_back('\n');
                    udpClient->sendMessage(sendRequest.data);
                }
                scheduleSend();  // schedule next check
            }
            else
            {
                LG_E("Send timer aborted");
            }*/
        });
}

void Client::udpReceive(const char* data, size_t length)
{
    if (length > 5)
    {
        CmdQueueData cmdData;
        cmdData.sendType = SendType::UDP;
        cmdData.data.insert(cmdData.data.end(), data, data + length);
        model.receiveQueue.enqueue(cmdData);
    }
}

void Client::tcpReceive(const char* data, size_t length)
{
    if (length > 5)
    {
        CmdQueueData cmdData;
        cmdData.sendType = SendType::TCP;
        cmdData.data.insert(cmdData.data.end(), data, data + length);
        model.receiveQueue.enqueue(cmdData);
    }
}

}  // namespace sphyc