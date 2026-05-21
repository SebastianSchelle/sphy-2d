#include <client.hpp>
#include <version.hpp>

namespace sphyc
{

Client::Client(cfg::ConfigManager& config,
               ConcurrentQueue<net::CmdQueueData>& modelSendQueue,
               ConcurrentQueue<net::CmdQueueData>& modelReceiveQueue)
    : modelSendQueue(modelSendQueue), modelReceiveQueue(modelReceiveQueue),
      config(config), sendTimer(ioContext)
{
}

Client::~Client()
{
    shutdown();
    if (ioThread.joinable())
    {
        ioThread.join();
    }
    {
        std::lock_guard<std::mutex> lock(lifecycleMutex);
        udpClient.reset();
        tcpClient.reset();
    }
    if (!spdlogShutdown.exchange(true))
    {
        spdlog::shutdown();
    }
}

void Client::shutdown(bool notifyModel)
{
    if (notifyModel)
    {
        if (!shutdownNotified.exchange(true) && shutdownCallback)
        {
            shutdownCallback();
        }
    }
    if (shuttingDown.exchange(true))
    {
        return;
    }
    LG_I("Shutting down client...");

    std::lock_guard<std::mutex> lock(lifecycleMutex);
    sendTimer.cancel();
    if (udpClient)
    {
        udpClient->close();
    }
    if (tcpClient)
    {
        tcpClient->close();
    }
    ioContext.stop();
}

void Client::wait()
{
    shutdown();
    if (ioThread.joinable())
    {
        ioThread.join();
    }
    {
        std::lock_guard<std::mutex> lock(lifecycleMutex);
        udpClient.reset();
        tcpClient.reset();
    }
    if (!spdlogShutdown.exchange(true))
    {
        spdlog::shutdown();
    }
}

void Client::connectToServer(const std::string& ipAddress,
                             int udpPortServ,
                             int tcpPortServ,
                             int udpPortCli,
                             const std::string& token)
{
    const uint32_t generation = ++connectGeneration;
    // Tear down old transport without notifying model — reconnect is intentional.
    shutdown(/*notifyModel=*/false);
    if (ioThread.joinable())
    {
        ioThread.join();
    }
    {
        std::lock_guard<std::mutex> lock(lifecycleMutex);
        udpClient.reset();
        tcpClient.reset();
        ioContext.restart();
    }

    shuttingDown = false;
    shutdownNotified = false;

    try
    {
        udpClient = std::make_unique<net::UdpClient>(
            ioContext,
            udpPortCli,
            udp::endpoint(boost::asio::ip::make_address(ipAddress), udpPortServ),
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
            tcp::endpoint(boost::asio::ip::make_address(ipAddress), tcpPortServ),
            std::bind(&Client::tcpReceive, this, std::placeholders::_1),
            [this, generation]()
            { connectionClosedClb(generation); });
        LG_D("Setup tcp socket to server at {}:{} on port {}",
             ipAddress,
             tcpPortServ,
             udpPortCli);
    }
    catch (const std::exception& e)
    {
        LG_E("Failed to connect to server at {}:{} — {}",
             ipAddress,
             tcpPortServ,
             e.what());
        shuttingDown = true;
        return;
    }

    boost::asio::post(ioContext, [this, token]() { scheduleSend(token); });
    ioThread = std::thread([this]() { ioContext.run(); });
}

void Client::scheduleSend(const std::string& token)
{
    sendTimer.expires_after(std::chrono::milliseconds(10));
    sendTimer.async_wait(
        [this, token](boost::system::error_code ec)
        {
            if (!ec)
            {
                net::CmdQueueData sendData;
                while (modelSendQueue.try_dequeue(sendData))
                {
                    if (sendData.sendType == net::SendType::UDP)
                    {
                        if (udpClient)
                        {
                            bitsery::Serializer<OutputAdapter> cmdser(
                                OutputAdapter(sendData.data));
                            cmdser.text1b(token, 16);
                            udpClient->sendMessage(sendData.data);
                        }
                    }
                    else if (sendData.sendType == net::SendType::TCP)
                    {
                        if (tcpClient)
                        {
                            tcpClient->sendMessage(sendData.data);
                        }
                    }
                }
                if (!shuttingDown.load())
                {
                    scheduleSend(token);  // schedule next check
                }
            }
            else
            {
                if (ec == boost::asio::error::operation_aborted
                    || shuttingDown.load())
                {
                    LG_D("Send timer cancelled");
                }
                else
                {
                    LG_E("Send timer aborted: {}", ec.message());
                }
            }
        });
}

void Client::udpReceive(const char* data, size_t length)
{
    if (shuttingDown.load())
    {
        return;
    }
    if (length > 5)
    {
        net::CmdQueueData cmdData;
        cmdData.sendType = net::SendType::UDP;
        cmdData.data.insert(cmdData.data.end(), data, data + length);
        modelReceiveQueue.enqueue(cmdData);
    }
}

void Client::tcpReceive(const net::CmdQueueData& cmdData)
{
    if (shuttingDown.load())
    {
        return;
    }
    modelReceiveQueue.enqueue(cmdData);
}

void Client::connectionClosedClb(uint32_t generation)
{
    if (generation != connectGeneration.load())
    {
        LG_D("Ignoring TCP close from previous connection");
        return;
    }
    if (!shuttingDown.load())
    {
        LG_W(
            "Server TCP connection lost/closed, shutting down client "
            "networking");
    }
    shutdown(/*notifyModel=*/true);
}

}  // namespace sphyc