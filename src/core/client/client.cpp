#include <client.hpp>

namespace sphyc
{

Client::Client(cfg::ConfigManager& config,
               ConcurrentQueue<net::CmdQueueData>& modelSendQueue,
               ConcurrentQueue<net::CmdQueueData>& modelReceiveQueue)
    : modelSendQueue(modelSendQueue), modelReceiveQueue(modelReceiveQueue), config(config), sendTimer(ioContext)
{
}

Client::~Client()
{
    shutdown();
    if (ioThread.joinable())
    {
        ioThread.join();
    }
    if (!spdlogShutdown.exchange(true))
    {
        spdlog::shutdown();
    }
}

void Client::shutdown()
{
    if (shuttingDown.exchange(true))
    {
        return;  // Already shutting down
    }
    LG_I("Shutting down client...");
    ioContext.stop();
}

void Client::wait()
{
    if (ioThread.joinable())
    {
        ioThread.join();
    }
    if (!spdlogShutdown.exchange(true))
    {
        spdlog::shutdown();
    }
}

void Client::connectToServer(const std::string& token,
                             const std::string& ipAddress,
                             int udpPortServ,
                             int tcpPortServ,
                             int udpPortCli)
{
    clientInfo.token = token;
    clientInfo.name = "Client";

    // tcpClient = std::make_unique<TcpClient>(ioContext, portTcp);
    udpClient = std::make_unique<net::UdpClient>(
        ioContext,
        udpPortCli,
        udp::endpoint(boost::asio::ip::address::from_string(ipAddress),
                      udpPortServ),
        std::bind(&Client::udpReceive,
                  this,
                  std::placeholders::_2,
                  std::placeholders::_3));
    LG_D("Setup udp socket to server at {}:{} on port {}",
         ipAddress.c_str(),
         udpPortServ,
         udpPortCli);

    tcpClient = std::make_unique<net::TcpClient>(
        ioContext,
        tcp::endpoint(boost::asio::ip::address::from_string(ipAddress),
                      tcpPortServ),
        std::bind(&Client::tcpReceive,
                  this,
                  std::placeholders::_1,
                  std::placeholders::_2));
    LG_D("Setup tcp socket to server at {}:{} on port {}",
         ipAddress,
         tcpPortServ,
         udpPortCli);

    // ioContext is already running in ioThread for signal handling
    // No need to start a new thread

    CMDAT_PREP(net::SendType::TCP, prot::cmd::CONNECT, 0)
    cmdser.text1b(token, 16);
    cmdser.value2b((uint16_t)udpPortCli);
    CMDAT_FIN()
    modelSendQueue.enqueue(cmdData);

    // Post scheduleSend onto the io_context so the timer is armed on the
    // correct executor (same thread that runs io_context.run()).
    ioContext.post([this]() { scheduleSend(); });
    // Start ioContext in background thread for signal handling
    ioThread = std::thread([this]() { ioContext.run(); });
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
                while (modelSendQueue.try_dequeue(sendData))
                {
                    if (sendData.sendType == net::SendType::UDP)
                    {
                        bitsery::Serializer<OutputAdapter> cmdser(
                            OutputAdapter(sendData.data));
                        cmdser.text1b(clientInfo.token, 16);
                        udpClient->sendMessage(sendData.data);
                    }
                    else if (sendData.sendType == net::SendType::TCP)
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
        modelReceiveQueue.enqueue(cmdData);
    }
}

void Client::tcpReceive(const char* data, size_t length)
{
    if (length >= 5)
    {
        net::CmdQueueData cmdData;
        cmdData.sendType = net::SendType::TCP;
        cmdData.data.insert(cmdData.data.end(), data, data + length);
        modelReceiveQueue.enqueue(cmdData);
    }
}

}  // namespace sphyc