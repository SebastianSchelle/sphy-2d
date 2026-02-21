#include <client.hpp>

namespace sphyc
{

Client::Client(cfg::ConfigManager& config)
    : model(), config(config), sendTimer(ioContext)
{
}

Client::~Client()
{
    shutdown();
    // Ensure threads are joined in destructor
    model.wait();
    if (ioThread.joinable())
    {
        ioThread.join();
    }
    // Shutdown spdlog only once
    if (!spdlogShutdown.exchange(true))
    {
        spdlog::shutdown();
    }
}

void Client::startClient()
{
    // Start ioContext in background thread for signal handling
    ioThread = std::thread([this]() { ioContext.run(); });

    model.start();
}

void Client::shutdown()
{
    if (shuttingDown.exchange(true))
    {
        return;  // Already shutting down
    }
    LG_I("Shutting down client...");
    ioContext.stop();  // Stop all IO operations (will cause ioContext.run() to
                       // return)
    model.stop();      // Stop model thread (sets flag, doesn't join)
    // Don't join threads here - signal handler might be called from ioThread
    // itself Threads will be joined in wait() or destructor
}

void Client::wait()
{
    // Wait for model thread to finish (will finish when shutdown is called)
    model.wait();
    // Now join ioThread (ioContext.run() has returned after stop())
    if (ioThread.joinable())
    {
        ioThread.join();
    }
    // Shutdown spdlog only once
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
                  std::placeholders::_1,
                  std::placeholders::_2));
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
    model.sendQueue.enqueue(cmdData);

    // Post scheduleSend onto the io_context so the timer is armed on the
    // correct executor (same thread that runs io_context.run()).
    scheduleSend();
    // Don't join ioThread here - it's already running and will be joined in
    // shutdown()
}

void Client::scheduleSend()
{
    LG_W("Scheduling send");
    sendTimer.expires_after(std::chrono::milliseconds(10));
    sendTimer.async_wait(
        [this](boost::system::error_code ec)
        {
            LG_W("Send timer async_wait callback");
            if (!ec)
            {
                LG_W("Sending timer expired");
                net::CmdQueueData sendData;
                while (model.sendQueue.try_dequeue(sendData))
                {
                    LG_I("Sending command");
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