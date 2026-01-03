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
    debug::createLogger("logs/logClient.txt", logLevel);

    // Setup for testing
    clientInfo.token = "1234abcd1234abcd";

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
    udpClient = std::make_unique<net::UdpClient>(
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

    tcpClient = std::make_unique<net::TcpClient>(
        ioContext,
        tcp::endpoint(boost::asio::ip::address::from_string(serverIp),
                      servPortTcp),
        std::bind(&Client::tcpReceive,
                  this,
                  std::placeholders::_1,
                  std::placeholders::_2));
    LG_D("Setup tcp socket to server at {}:{} on port {}",
         serverIp,
         servPortTcp,
         portUdp);

    ioThread = std::thread([this]() { ioContext.run(); });

    CMDAT_PREP(net::SendType::TCP, prot::cmd::CONNECT, 0)
    std::string token = "1234abcd1234abcd";
    cmdser.text1b(token, token.size());
    cmdser.value2b((uint16_t)portUdp);
    CMDAT_FIN()
    model.sendQueue.enqueue(cmdData);

    scheduleSend();
}

void Client::scheduleSend()
{
    sendTimer.expires_after(std::chrono::milliseconds(10));
    sendTimer.async_wait(
        [this](boost::system::error_code ec)
        {
            if (!ec)
            {
                net::CmdQueueData sendData;
                while (model.sendQueue.try_dequeue(sendData))
                {
                    if(sendData.sendType == net::SendType::UDP)
                    {
                        bitsery::Serializer<OutputAdapter> cmdser(OutputAdapter(sendData.data));
                        cmdser.text1b(clientInfo.token, 16);
                        udpClient->sendMessage(sendData.data);
                    }
                    else if(sendData.sendType == net::SendType::TCP)
                    {
                        tcpClient->sendMessage(sendData.data);
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

void Client::udpReceive(const char* data, size_t length)
{
    if (length > 5)
    {
        net::CmdQueueData cmdData;
        cmdData.sendType = net::SendType::UDP;
        cmdData.data.insert(cmdData.data.end(), data, data + length);
        model.receiveQueue.enqueue(cmdData);
    }
}

void Client::tcpReceive(const char* data, size_t length)
{
    if (length >= 5)
    {
        net::CmdQueueData cmdData;
        cmdData.sendType = net::SendType::TCP;
        cmdData.data.insert(cmdData.data.end(), data, data + length);
        model.receiveQueue.enqueue(cmdData);
    }
}

}  // namespace sphyc