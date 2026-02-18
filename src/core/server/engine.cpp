#include "bitsery/serializer.h"
#include "std-inc.hpp"
#include <engine.hpp>
#include <protocol.hpp>
#include <server.hpp>

namespace sphys
{

Engine::Engine(const sphy::CmdLinOptionsServer& options)
    : options(options), state(EngineState::Init), saveConfig(),
      saveFolder(options.savedir)
{
}

Engine::~Engine()
{
    stopRequested = true;
    if (engineThread.joinable())
    {
        engineThread.join();
    }
}

void Engine::start()
{
    registerClient("1234abcd1234abcd", "Test Client");
    engineThread = std::thread([this]() { engineLoop(); });
}

void Engine::stop()
{
    stopRequested = true;
    if (engineThread.joinable())
    {
        engineThread.join();
    }
}

void Engine::engineLoop()
{
    while (!stopRequested)
    {
        switch (state)
        {
            case EngineState::Init:
                state = EngineState::LoadMods;
                break;
            case EngineState::LoadMods:
                if (loadMods())
                {
                    startFromFolder();
                }
                else
                {
                    state = EngineState::Error;
                }
                break;
            case EngineState::LoadWorld:
                if (loadFromFolder())
                {
                    state = EngineState::Running;
                }
                else
                {
                    state = EngineState::Error;
                }
                break;
            case EngineState::CreateWorld:
                if (createFromConfig())
                {
                    state = EngineState::Running;
                }
                else
                {
                    state = EngineState::Error;
                }
                break;
            case EngineState::Running:
                // test
                for (int i = 0; i < activeClientHandles.size(); i++)
                {
                    net::ClientInfoHandle handle = activeClientHandles[i];
                    net::ClientInfo* clientInfo = clientLib.getItem(handle);
                    if (clientInfo)
                    {
                        CMDAT_PREP(net::SendType::UDP, prot::cmd::LOG, 0)
                        std::string str = "Hello World!";
                        cmdser.text1b(str, str.size());
                        cmdData.udpEndpoint = clientInfo->udpEndpoint;
                        CMDAT_FIN()
                        sendQueue.enqueue(cmdData);
                    }
                }
                break;
            case EngineState::Paused:
                break;
            case EngineState::Stopped:
                break;
            case EngineState::Error:
                LG_E("Engine error");
                stopRequested = true;
                break;
        }


        net::CmdQueueData recQueueData;
        while (receiveQueue.try_dequeue(recQueueData))
        {
            parseCommand(recQueueData);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (state == EngineState::Running || state == EngineState::Paused)
    {
        LG_I("Shutdown requested, saving game...");
        saveGame();
    }
}


void Engine::startFromFolder()
{
    std::string saveFld = saveFolder + "/save-data";
    if (fs::exists(saveFld))
    {
        state = EngineState::LoadWorld;
    }
    else
    {
        state = EngineState::CreateWorld;
    }
}

bool Engine::loadFromFolder()
{
    if(!world.createFromSave(saveConfig, saveFolder))
    {
        LG_E("Failed to load world from save");
        return false;
    }
    return true;
}

bool Engine::createFromConfig()
{
    std::string configPath = saveFolder + "/config.yaml";
    if (fs::exists(configPath))
    {
        saveConfig.clear();
        saveConfig.addDefs(configPath);
        if(!world.createFromConfig(saveConfig))
        {
            LG_E("Failed to create world from config");
            return false;
        }
    }
    else
    {
        LG_E("Config file not found");
        return false;
    }
    LG_I("Creating from config: {}", configPath);
    return true;
}

bool Engine::loadMods()
{
    LG_I("Loading mods");
    std::string modListPath = saveFolder + "/modlist.txt";

    std::vector<std::string> modList;
    if (!modManager.parseModList(modListPath, modList))
    {
        LG_E("Failed to parse mod list");
        return false;
    }
    if (!modManager.checkDependencies(modList, "modules"))
    {
        LG_E("Failed to check dependencies");
        return false;
    }
    mod::PtrHandles ptrHandles{.luaInterpreter = &luaInterpreter};
    if (!modManager.loadMods(ptrHandles))
    {
        LG_E("Failed to load mods");
    }
    return true;
}

void Engine::saveGame()
{
    world.saveWorld(saveFolder);
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
                    if (token.length() == 16)
                    {
                        net::ClientInfoHandle handle =
                            clientLib.getHandle(token);
                        if (handle.isValid())
                        {
                            auto address = cmdData.tcpConnection->socket()
                                               .local_endpoint()
                                               .address();
                            activeClientHandles.push_back(handle);
                            net::ClientInfo* clientInfo =
                                clientLib.getItem(handle);
                            clientInfo->portUdp = portUdp;
                            clientInfo->address = address;
                            clientInfo->udpEndpoint =
                                udp::endpoint(address, portUdp);
                            clientInfo->connection = cmdData.tcpConnection;
                            LG_I(
                                "Client authenticated. ip={}, udp port={}, "
                                "token={}",
                                address.to_string(),
                                portUdp,
                                token);
                            {
                                CMDAT_PREP(net::SendType::TCP,
                                           prot::cmd::CONNECT,
                                           CMD_FLAG_RESP)
                                CMDAT_FIN()
                                cmdData.tcpConnection = clientInfo->connection;
                                sendQueue.enqueue(cmdData);
                            }
                            return;
                        }
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

template class con::ItemLib<net::ClientInfo>;