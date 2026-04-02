#include "bitsery/serializer.h"
#include "ecs.hpp"
#include "rerun.hpp"
#include "std-inc.hpp"
#include <comp-ident.hpp>
#include <comp-phy.hpp>
#include <engine.hpp>
#include <protocol.hpp>
#include <server.hpp>
#include <version.hpp>

namespace sphys
{

Engine::Engine(const sphy::CmdLinOptionsServer& options)
    : options(options), state(EngineState::Init), saveConfig(),
      saveFolder(options.savedir), rerunStream("sphy-2d"), commandManager()
{
    ptrHandle = std::make_shared<ecs::PtrHandle>();
    ptrHandle->ecs = &ecs;
    ptrHandle->world = &world;
    ptrHandle->engine = this;
    ptrHandle->systems = &ecs.getRegisteredSystems();
    ptrHandle->registry = &ecs.getRegistry();

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
    cFac->registerComponent<ecs::Transform>("trans");
    cFac->registerComponent<ecs::PhysicsBody>("phy");
    cFac->registerComponent<ecs::AssetId>("asset-id");
    registerConsoleCommands();

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
    tim::Timepoint lastSaveTime = tim::getCurrentTimeU();
    tim::Timepoint lastUpdateTime = tim::getCurrentTimeU();

    while (!stopRequested)
    {
        tim::Timepoint now = tim::getCurrentTimeU();
        float dt = tim::durationU(lastUpdateTime, now) / 1000000.0f;
        lastUpdateTime = now;
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
                        "test", 0, ecs::Transform{glm::vec2{0.0f, 0.0f}, 0.0f});
                    spawnEntityFromAsset(
                        "test",
                        1,
                        ecs::Transform{glm::vec2{-3.0f, 5.0f}, 2.0f});
                    spawnEntityFromAsset(
                        "test",
                        2,
                        ecs::Transform{glm::vec2{-3.0f, 5.0f}, 2.0f});
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
                DO_PERIODIC(lastSaveTime, TIM_5M, saveGame)
                update(dt);
                // test
                /*for (int i = 0; i < activeClientHandles.size(); i++)
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
                }*/
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
            system.function(entity, dt, ptrHandle);
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

void Engine::parseCommand(const net::CmdQueueData& cmdData)
{
    uint16_t cmd;
    uint8_t flags;
    uint16_t len;
    std::string uuid;
    std::optional<udp::endpoint> udpEndpoint;
    std::shared_ptr<net::TcpConnection> tcpConnection;

    try
    {
        const std::vector<uint8_t>& data = cmdData.data;
        bitsery::Deserializer<InputAdapter> cmddes(
            InputAdapter{data.begin(), data.size()});

        if (cmdData.sendType == net::SendType::UDP)
        {
            cmddes.text1b(uuid, 16);
            udpEndpoint = cmdData.udpEndpoint;
        }
        else if (cmdData.sendType == net::SendType::TCP)
        {
            tcpConnection = cmdData.tcpConnection;
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
                long d = tim::nowU();
                CMDAT_PREP(
                    net::SendType::UDP, prot::cmd::TIME_SYNC, CMD_FLAG_RESP)
                cmdser.value8b(d);
                CMDAT_FIN()
                if (udpEndpoint)
                {
                    cmdData.udpEndpoint = *udpEndpoint;
                    cmdData.udpPort = udpEndpoint->port();
                }
                sendQueue.enqueue(cmdData);
                break;
            }
            case prot::cmd::VERSION_CHECK:
            {
                CMDAT_PREP(
                    net::SendType::TCP, prot::cmd::VERSION_CHECK, CMD_FLAG_RESP)
                cmdser.value2b(version::MAJOR);
                cmdser.value2b(version::MINOR);
                cmdser.value2b(version::PATCH);
                CMDAT_FIN()
                cmdData.tcpConnection = tcpConnection;
                sendQueue.enqueue(cmdData);
                break;
            }
            case prot::cmd::AUTHENTICATE:
            {
                if (cmdData.sendType == net::SendType::TCP)
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
                            clientInfo->connection->setClientInfoHandle(handle);
                            LG_I(
                                "Client authenticated. ip={}, udp port={}, "
                                "token={}",
                                address.to_string(),
                                portUdp,
                                token);
                            {
                                CMDAT_PREP(net::SendType::TCP,
                                           prot::cmd::AUTHENTICATE,
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
            case prot::cmd::WORLD_INFO:
            {
                LG_I("Sending world info");
                CMDAT_PREP(
                    net::SendType::TCP, prot::cmd::WORLD_INFO, CMD_FLAG_RESP)
                auto worldShape = world.getWorldShape();
                cmdser.value4b(worldShape.numSectorX);
                cmdser.value4b(worldShape.numSectorY);
                cmdser.value4b(worldShape.sectorSize);
                CMDAT_FIN()
                cmdData.tcpConnection = tcpConnection;
                sendQueue.enqueue(cmdData);
                break;
            }
            case prot::cmd::CONSOLE_CMD:
            {
                if (cmdData.sendType == net::SendType::TCP)
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
                                    response =
                                        commandManager.executeCommand(data);
                                }
                                catch (const std::exception& e)
                                {
                                    response =
                                        "Failed: " + std::string(e.what());
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

                    CMDAT_PREP(net::SendType::TCP,
                               prot::cmd::CONSOLE_CMD,
                               CMD_FLAG_RESP)
                    cmdser.text1b(response, response.size());
                    CMDAT_FIN()
                    cmdData.tcpConnection = tcpConnection;
                    sendQueue.enqueue(cmdData);
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

}  // namespace sphys

template class con::ItemLib<net::ClientInfo>;