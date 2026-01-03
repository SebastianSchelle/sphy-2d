#include "bitsery/serializer.h"
#include "std-inc.hpp"
#include <engine.hpp>
#include <protocol.hpp>
#include <server.hpp>

namespace sphys
{

Engine::Engine() {}

Engine::~Engine() {}

void Engine::start()
{
    registerClient("1234abcd1234abcd", "Test Client");
    engineThread = std::thread([this]() { engineLoop(); });
}

void Engine::engineLoop()
{
    while (true)
    {
        net::CmdQueueData recQueueData;
        while (receiveQueue.try_dequeue(recQueueData))
        {
            parseCommand(recQueueData);
        }

        // test
        for(int i = 0; i < activeClientIdxs.size(); i++)
        {
            int idx = activeClientIdxs[i];
            net::ClientInfo *clientInfo = clientLib.getItem(idx);
            if(clientInfo)
            {
                CMDAT_PREP(net::SendType::UDP, prot::cmd::LOG, 0)
                std::string str = "Hello World!";
                cmdser.text1b(str, str.size());
                cmdData.udpEndpoint = clientInfo->udpEndpoint;
                CMDAT_FIN()
                sendQueue.enqueue(cmdData);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void Engine::registerClient(const std::string& uuid, const std::string& name)
{
    net::ClientInfo clientInfo;
    clientInfo.token = uuid;
    clientInfo.name = name;
    clientInfo.portUdp = 0;
    clientInfo.address = asio::ip::address::from_string("0.0.0.0");
    clientLib.addItem(uuid, clientInfo);
}

void Engine::parseCommand(const net::CmdQueueData& cmdData)
{
    uint16_t cmd;
    uint8_t flags;
    uint16_t len;
    std::string uuid;

    try
    {
        const std::vector<uint8_t>& data = cmdData.data;
        bitsery::Deserializer<InputAdapter> cmddes(
            InputAdapter{data.begin(), data.size()});

        if (cmdData.sendType == net::SendType::UDP)
        {
            cmddes.text1b(uuid, 16);
        }
        cmddes.value2b(cmd);
        cmddes.value1b(flags);
        cmddes.value2b(len);

        switch (cmd)
        {
            case prot::cmd::LOG:
            {
                std::string str;
                cmddes.text1b(str, 256);
                if (cmdData.sendType == net::SendType::UDP)
                {
                    LG_I("Log from uuid={}: {}", uuid, str);
                }
                else if (cmdData.sendType == net::SendType::TCP)
                {
                    LG_I("Log from tcp {}", str);
                }
                break;
            }
            case prot::cmd::TIME_SYNC:
            {
                tim::Duration d = tim::getCurrentTimeU() - tim::epoch;
                int64_t secs = d.total_seconds();
                int32_t usec = d.fractional_seconds();
                CMDAT_PREP(
                    net::SendType::UDP, prot::cmd::TIME_SYNC, CMD_FLAG_RESP)
                cmdser.value8b(secs);
                cmdser.value4b(usec);
                CMDAT_FIN()
                sendQueue.enqueue(cmdData);
                break;
            }
            case prot::cmd::CONNECT:
            {
                if (cmdData.sendType == net::SendType::TCP)
                {
                    std::string token;
                    uint16_t portUdp;
                    cmddes.text1b(token, 16);
                    cmddes.value2b(portUdp);
                    int idx = clientLib.getIndex(token);
                    if (idx != -1 && token.length() == 16)
                    {
                        auto address = cmdData.tcpConnection->socket().local_endpoint().address();
                        activeClientIdxs.push_back(idx);
                        net::ClientInfo *clientInfo = clientLib.getItem(idx);
                        clientInfo->portUdp = portUdp;
                        clientInfo->address = address;
                        clientInfo->udpEndpoint = udp::endpoint(address, portUdp);
                        clientInfo->connection = cmdData.tcpConnection;
                        LG_I("Client authenticated. ip={}, udp port={}, token={}", address.to_string(), portUdp, token);
                        {
                            CMDAT_PREP(net::SendType::TCP, prot::cmd::CONNECT, CMD_FLAG_RESP)
                            CMDAT_FIN()
                            cmdData.tcpConnection = clientInfo->connection;
                            sendQueue.enqueue(cmdData);
                        }
                        return;
                    }
                    cmdData.tcpConnection->close();
                        LG_E("Client not authenticated. Close connection. {}",
                             token);
                }
                break;
            }
            default:
                break;
        }
    }
    catch (const std::exception& e)
    {
        LG_E("Error parsing command: {}", e.what());
    }
}

}  // namespace sphys
