#include "bitsery/serializer.h"
#include "ecs.hpp"
#include "rerun.hpp"
#include "std-inc.hpp"
#include <comp-ident.hpp>
#include <comp-phy.hpp>
#include <engine-impl.hpp>
#include <protocol.hpp>
#include <server.hpp>
#include <version.hpp>

namespace sphys
{

Engine::Engine(const sphy::CmdLinOptionsServer& options,
               cfg::ConfigManager& config)
    : options(options), config(config), state(EngineState::Init), saveConfig(),
      saveFolder(options.savedir), rerunStream("sphy-2d"), commandManager()
{
    ptrHandle = std::make_shared<ecs::PtrHandle>();
    ptrHandle->ecs = &ecs;
    ptrHandle->world = &world;
    ptrHandle->engine = this;
    ptrHandle->systems = &ecs.getRegisteredSystems();
    ptrHandle->registry = &ecs.getRegistry();

    slowDumpUs = 1000 * CFG_UINT(config, "engine", "slow-dump-ms");
    // rerunStream.spawn().exit_on_failure();
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
    auto cFac = &assetFactory.componentFactory;
    cFac->registerComponent<ecs::Transform>();
    cFac->registerComponent<ecs::PhysicsBody>();
    cFac->registerComponent<ecs::AssetId>();
    cFac->registerComponent<ecs::PhyThrust>();

    registerConsoleCommands();

    registerSlowDumpComponent<ecs::Transform>();

    registerClient(
        "1234abcd1234abcd", "Test Client", net::ClientFlags{.enConsole = 1});
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
    long lastSaveTime = tim::nowU();
    long lastUpdateTime = tim::nowU();

    while (!stopRequested)
    {
        long nowU = tim::nowU();
        float dt = (nowU - lastUpdateTime) / 1000000.0f;

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
                    spawnEntityFromAsset(
                        "test", 27, ecs::Transform{glm::vec2{0.0f, 0.0f}, 0.0f});
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
            {
                update(dt);
                runSlowClientDump(nowU);
                DO_PERIODIC_U_EXTNOW(lastSaveTime, TIM_5M, nowU, saveGame)
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
            parseCommandData(recQueueData);
        }
        lastUpdateTime = nowU;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (state == EngineState::Running || state == EngineState::Paused)
    {
        LG_I("Shutdown requested, saving game...");
        saveGame();
    }
}

void Engine::update(float dt)
{
    for (int i = 0; i < globalEntityIds.size(); i++)
    {
        ecs::EntityId entityId = globalEntityIds[i];
        entt::entity entity = globalEntities[i];
        for (auto system : *ptrHandle->systems)
        {
            system.function(entity, entityId, dt, ptrHandle);
        }
    }
    world.update(dt, ptrHandle);

    // Debugging
    // std::vector<rerun::Position2D> positions = {};
    // auto view = ptrHandle->registry->view<const ecs::EntityId,
    //                                       const ecs::SectorId,
    //                                       const ecs::Transform>();
    // view.each(
    //     [&positions, this](const auto entity,
    //                        const auto& entityId,
    //                        const auto& sectorId,
    //                        const auto& trans)
    //     {
    //         world::Sector* sector = world.getSector(sectorId.id);
    //         if (sector)
    //         {
    //             positions.push_back({trans.pos.x + sector->getWorldPosX(),
    //                                  trans.pos.y + sector->getWorldPosY()});
    //         }
    //     });

    // rerunStream.log("points", rerun::Points2D(positions));
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
    if (!world.createFromSave(saveConfig, saveFolder))
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
        if (!world.createFromConfig(saveConfig))
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
    mod::PtrHandles ptrHandles{.luaInterpreter = &luaInterpreter,
                               .assetFactory = &assetFactory};
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

void Engine::registerClient(const std::string& uuid,
                            const std::string& name,
                            net::ClientFlags flags)
{
    net::ClientInfo clientInfo;
    clientInfo.token = uuid;
    clientInfo.name = name;
    clientInfo.flags = flags;
    clientInfo.portUdp = 0;
    clientInfo.address = asio::ip::address::from_string("0.0.0.0");
    clientLib.addItem(uuid, clientInfo);
}

void Engine::parseCommandData(const net::CmdQueueData& cmdData)
{
    if (cmdData.sendType == net::SendType::TCP && cmdData.tcpDisconnected)
    {
        handleTcpDisconnect(cmdData.tcpConnection);
        return;
    }

    try
    {
        std::string token;
        const udp::endpoint* udpEndpoint;
        std::shared_ptr<net::TcpConnection> tcpConnection;
        net::ClientInfo* clientInfo = nullptr;

        const std::vector<uint8_t>& data = cmdData.data;
        bitsery::Deserializer<InputAdapter> cmddes(
            InputAdapter{data.begin(), data.size()});

        if (cmdData.sendType == net::SendType::UDP)
        {
            cmddes.text1b(token, 16);
            if (token.length() != 16)
            {
                LG_W("Invalid token length received from ip {}",
                     cmdData.udpEndpoint.address().to_string());
                return;
            }
            net::ClientInfoHandle handle = clientLib.getHandle(token);
            if (!handle.isValid())
            {
                LG_W("Invalid token received from ip {}",
                     cmdData.udpEndpoint.address().to_string());
                return;
            }
            clientInfo = clientLib.getItem(handle);
            udpEndpoint = &cmdData.udpEndpoint;
        }
        else if (cmdData.sendType == net::SendType::TCP)
        {
            tcpConnection = cmdData.tcpConnection;
            net::ClientInfoHandle handle = tcpConnection->getClientInfoHandle();
            if (handle.isValid())
            {
                clientInfo = clientLib.getItem(handle);
            }
        }

        while (cmddes.adapter().currentReadPos() <= data.size() - 5)
        {
            uint16_t cmd;
            uint8_t flags;
            uint16_t len;
            cmddes.value2b(cmd);
            cmddes.value1b(flags);
            cmddes.value2b(len);
            size_t dataStartPos = cmddes.adapter().currentReadPos();

            if (cmddes.adapter().currentReadPos() + len > data.size())
            {
                LG_E("Command data too short");
                break;
            }
            parseCommand(cmddes,
                         token,
                         udpEndpoint,
                         tcpConnection,
                         clientInfo,
                         cmdData.sendType,
                         cmd,
                         flags,
                         len);
            cmddes.adapter().currentReadPos(dataStartPos + len);
        }
    }
    catch (const std::exception& e)
    {
        LG_E("Error parsing command message: {}", e.what());
    }
}

void Engine::parseCommand(bitsery::Deserializer<InputAdapter>& cmddes,
                          std::string& uuid,
                          const udp::endpoint* udpEndpoint,
                          std::shared_ptr<net::TcpConnection>& tcpConnection,
                          net::ClientInfo* clientInfo,
                          net::SendType sendType,
                          uint16_t cmd,
                          uint8_t flags,
                          uint16_t len)
{
    switch (cmd)
    {
        case prot::cmd::LOG:
        {
            if (sendType == net::SendType::UDP && (flags & CMD_FLAG_RESP) == 0)
            {
                std::string str;
                cmddes.text1b(str, 256);
                if (sendType == net::SendType::UDP)
                {
                    LG_I("UDP Log from {}: {}", clientInfo->name, str);
                }
                else if (sendType == net::SendType::TCP)
                {
                    LG_I("TCP Log from {}: {}", clientInfo->name, str);
                }
            }
            break;
        }
        case prot::cmd::TIME_SYNC:
        {
            if (sendType == net::SendType::UDP && (flags & CMD_FLAG_RESP) == 0)
            {
                long d = tim::nowU();
                prot::writeMessageUdp(
                    sendQueue,
                    udpEndpoint,
                    [this, d](bitsery::Serializer<OutputAdapter>& cmdser)
                    {
                        prot::writeCommand(
                            cmdser,
                            prot::cmd::TIME_SYNC,
                            CMD_FLAG_RESP,
                            [this,
                             d](bitsery::Serializer<OutputAdapter>& cmdser)
                            { cmdser.value8b(d); });
                    });
            }
            break;
        }
        case prot::cmd::VERSION_CHECK:
        {
            if (sendType == net::SendType::TCP && (flags & CMD_FLAG_RESP) == 0)
            {
                prot::writeMessageTcp(
                    sendQueue,
                    tcpConnection,
                    [this](bitsery::Serializer<OutputAdapter>& cmdser)
                    {
                        prot::writeCommand(
                            cmdser,
                            prot::cmd::VERSION_CHECK,
                            CMD_FLAG_RESP,
                            [this](bitsery::Serializer<OutputAdapter>& cmdser)
                            {
                                cmdser.value2b(version::MAJOR);
                                cmdser.value2b(version::MINOR);
                                cmdser.value2b(version::PATCH);
                            });
                    });
            }
            break;
        }
        case prot::cmd::AUTHENTICATE:
        {
            if (sendType == net::SendType::TCP && (flags & CMD_FLAG_RESP) == 0)
            {
                LG_I("Authentication request from tcp");
                std::string token;
                uint16_t portUdp;
                uint16_t major;
                uint16_t minor;
                uint16_t patch;
                cmddes.value2b(major);
                cmddes.value2b(minor);
                cmddes.value2b(patch);
                cmddes.text1b(token, 16);
                cmddes.value2b(portUdp);
                if (token.length() == 16 && major == version::MAJOR)
                {
                    net::ClientInfoHandle handle = clientLib.getHandle(token);
                    if (handle.isValid())
                    {
                        auto address =
                            tcpConnection->socket().local_endpoint().address();
                        activeClientHandles.push_back(handle);
                        net::ClientInfo* clientInfo = clientLib.getItem(handle);
                        clientInfo->portUdp = portUdp;
                        clientInfo->address = address;
                        clientInfo->udpEndpoint =
                            udp::endpoint(address, portUdp);
                        clientInfo->connection = tcpConnection;
                        clientInfo->connection->setClientInfoHandle(handle);
                        LG_I(
                            "Client authenticated. ip={}, udp port={}, "
                            "token={}",
                            address.to_string(),
                            portUdp,
                            token);
                        {
                            prot::writeMessageTcp(
                                sendQueue,
                                tcpConnection,
                                [this](
                                    bitsery::Serializer<OutputAdapter>& cmdser)
                                {
                                    prot::writeCommand(
                                        cmdser,
                                        prot::cmd::AUTHENTICATE,
                                        CMD_FLAG_RESP,
                                        [this](
                                            bitsery::Serializer<OutputAdapter>&
                                                cmdser) {});
                                });
                        }
                        return;
                    }
                }
                tcpConnection->close();
                LG_E("Client not authenticated. Close connection. {}", token);
            }
            break;
        }
        case prot::cmd::WORLD_INFO:
        {
            if (sendType == net::SendType::TCP && (flags & CMD_FLAG_RESP) == 0)
            {
                LG_I("Sending world info");
                prot::writeMessageTcp(
                    sendQueue,
                    tcpConnection,
                    [this](bitsery::Serializer<OutputAdapter>& cmdser)
                    {
                        prot::writeCommand(
                            cmdser,
                            prot::cmd::WORLD_INFO,
                            CMD_FLAG_RESP,
                            [this](bitsery::Serializer<OutputAdapter>& cmdser)
                            {
                                auto worldShape = world.getWorldShape();
                                cmdser.value4b(worldShape.numSectorX);
                                cmdser.value4b(worldShape.numSectorY);
                                cmdser.value4b(worldShape.sectorSize);
                            });
                    });
            }
            break;
        }
        case prot::cmd::CONSOLE_CMD:
        {
            if (sendType == net::SendType::TCP && (flags & CMD_FLAG_RESP) == 0)
            {
                string data;
                string response;
                cmddes.text1b(data, len);
                net::ClientInfoHandle clientInfoHandle =
                    tcpConnection->getClientInfoHandle();
                if (clientInfoHandle.isValid())
                {
                    net::ClientInfo* clientInfo =
                        clientLib.getItem(clientInfoHandle);
                    if (clientInfo)
                    {
                        if (clientInfo->flags.enConsole)
                        {
                            try
                            {
                                response = commandManager.executeCommand(data);
                            }
                            catch (const std::exception& e)
                            {
                                response = "Failed: " + std::string(e.what());
                            }
                        }
                        else
                        {
                            response = "Failed: Client " + clientInfo->name
                                       + " is not a console";
                        }
                    }
                    else
                    {
                        response = "Failed: Client info not found";
                    }
                }
                else
                {
                    response = "Failed: Client info handle not valid";
                }

                prot::writeMessageTcp(
                    sendQueue,
                    tcpConnection,
                    [this, response](bitsery::Serializer<OutputAdapter>& cmdser)
                    {
                        prot::writeCommand(
                            cmdser,
                            prot::cmd::CONSOLE_CMD,
                            CMD_FLAG_RESP,
                            [this, response](
                                bitsery::Serializer<OutputAdapter>& cmdser)
                            { cmdser.text1b(response, response.size()); });
                    });
            }
            break;
        }
        case prot::cmd::REQ_ALL_COMPONENTS:
        {
            if ((flags & CMD_FLAG_RESP) == 0)
            {
                ecs::EntityId entityId;
                cmddes.value4b(entityId.index);
                cmddes.value2b(entityId.generation);
                sendAllComponents(entityId, tcpConnection);
            }
        }
        default:
            break;
    }
}

ecs::EntityId Engine::spawnEntityFromAsset(const std::string& assetId)
{
    ecs::EntityId ent = ecs.createEntity();
    if (!ecs.validId(ent))
    {
        LG_E("Failed to create entity");
        return ent;
    }
    ecs.spawnEntityFromAsset(ent, assetId, assetFactory);
    return ent;
}

ecs::EntityId Engine::spawnEntityFromAsset(const std::string& assetId,
                                           uint32_t sectorId,
                                           const ecs::Transform& transform)
{
    ecs::EntityId ent = spawnEntityFromAsset(assetId);
    if (!ecs.validId(ent))
    {
        LG_E("Failed to spawn entity");
        return ent;
    }
    world.moveEntityTo(ptrHandle, ent, sectorId, transform.pos, transform.rot);
    return ent;
}

void Engine::postWorldSetup() {}

void Engine::registerConsoleCommands()
{
    commandManager.registerCommand(
        {"ping"},
        [this](const std::vector<std::string>& arguments)
        { return "Ok: Pong"; });

    commandManager.registerCommand(
        {"log", "warn"},
        [this](const std::vector<std::string>& arguments)
        {
            LG_W("{}", arguments);
            return "Ok";
        },
        1);

    commandManager.registerCommand(
        {"log"},
        [this](const std::vector<std::string>& arguments)
        {
            LG_I("{}", arguments);
            return "Ok";
        },
        1);

    commandManager.registerCommand(
        {"help"},
        [this](const std::vector<std::string>& arguments)
        {
            if (arguments.empty())
            {
                return commandManager.help("");
            }
            else
            {
                return commandManager.help(arguments[0]);
            }
        });

    commandManager.registerCommand(
        {"asset", "list"},
        [this](const std::vector<std::string>& arguments)
        {
            return assetFactory.assetList(arguments.empty() ? "all"
                                                            : arguments[0]);
        });

    commandManager.registerCommand(
        {"asset", "info"},
        [this](const std::vector<std::string>& arguments)
        { return assetFactory.assetInfo(arguments[0]); },
        1);
}

void Engine::runSlowClientDump(long frameTime)
{
    for (int i = 0; i < activeClientHandles.size(); i++)
    {
        net::ClientInfoHandle handle = activeClientHandles[i];
        net::ClientInfo* clientInfo = clientLib.getItem(handle);
        DO_PERIODIC_U_EXTNOW(clientInfo->lastSlowDump,
                             slowDumpUs,
                             frameTime,
                             [&]()
                             {
                                 for (auto& component : slowDumpComponents)
                                 {
                                     component.function(clientInfo, ptrHandle);
                                 }
                             });
    }
}

void Engine::handleTcpDisconnect(
    const std::shared_ptr<net::TcpConnection>& conn)
{
    if (!conn)
        return;
    net::ClientInfoHandle handle = conn->getClientInfoHandle();
    conn->setClientInfoHandle(net::ClientInfoHandle::Invalid());
    if (!handle.isValid())
        return;
    const uint32_t hv = handle.value();
    for (auto it = activeClientHandles.begin();
         it != activeClientHandles.end();)
    {
        if (it->value() == hv)
            it = activeClientHandles.erase(it);
        else
            ++it;
    }
    if (net::ClientInfo* ci = clientLib.getItem(handle))
        ci->connection.reset();
    LG_I("TCP client disconnected (handle value={})", hv);
}

void Engine::sendAllComponents(ecs::EntityId entityId,
                               const std::shared_ptr<net::TcpConnection>& conn)
{
    entt::entity ent = ecs.getEntity(entityId);
    if (ent == entt::null)
    {
        // todo: reply needed in error case?
        return;
    }
    // todo: prevent exceeding tcp packet size
    prot::writeMessageTcp(
        sendQueue,
        conn,
        [this, entityId, ent](bitsery::Serializer<OutputAdapter>& cmdser)
        {
            prot::writeCommand(
                cmdser,
                prot::cmd::REQ_ALL_COMPONENTS,
                CMD_FLAG_RESP,
                [this, entityId, ent](
                    bitsery::Serializer<OutputAdapter>& cmdser)
                {
                    cmdser.value4b(entityId.index);
                    cmdser.value2b(entityId.generation);
                    auto& reg = ecs.getRegistry();
                    for (auto& [hash, helper] :
                         assetFactory.componentFactory.getComponentHelpers())
                    {
                        helper.serializeFromRegistry(reg, ent, cmdser);
                    }
                });
        });
}


}  // namespace sphys

template class con::ItemLib<net::ClientInfo>;