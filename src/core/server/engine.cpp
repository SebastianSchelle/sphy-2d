#include "bitsery/serializer.h"
#include "ecs.hpp"
#include "rerun.hpp"
#include "std-inc.hpp"
#include <comp-ident.hpp>
#include <comp-phy.hpp>
#include <components/comp-phy.hpp>
#include <engine-impl.hpp>
#include <protocol.hpp>
#include <server.hpp>
#include <sys-phy.hpp>
#include <version.hpp>

namespace sphys
{

Engine::Engine(const sphy::CmdLinOptionsServer& options,
               cfg::ConfigManager& config)
    : options(options), config(config), state(EngineState::Init), saveConfig(),
      saveFolder(options.savedir), rerunStream("sphy-2d-move-ctrl"),
      commandManager()
{
    ptrHandle = std::make_shared<ecs::PtrHandle>();
    ptrHandle->ecs = &ecs;
    ptrHandle->world = &world;
    ptrHandle->engine = this;
    ptrHandle->systems = &ecs.getRegisteredSystems();
    ptrHandle->registry = &ecs.getRegistry();
    ptrHandle->kpThrust =
        CFG_FLOAT(config, 25.0f, "engine", "physics", "kp-thrust");
    ptrHandle->kpTurn =
        CFG_FLOAT(config, 25.0f, "engine", "physics", "kp-turn");
    ptrHandle->angDrag =
        CFG_FLOAT(config, 0.1f, "engine", "physics", "ang-drag");
    ptrHandle->linDrag =
        CFG_FLOAT(config, 0.1f, "engine", "physics", "lin-drag");
    slowDumpUs = 1000 * CFG_UINT(config, 1000.0f, "engine", "slow-dump-ms");
    rerunStream.spawn().exit_on_failure();
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
    cFac->registerComponent<ecs::MoveCtrl>();

    ecs.registerSystem(ecs::sysMoveCtrl);
    ecs.registerSystem(ecs::sysPhyThrust);
    ecs.registerSystem(ecs::sysPhysics);

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
                        "test", 0, ecs::Transform{glm::vec2{0.0f, 0.0f}, 0.0f});
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
    rerunDebugMovePhy();
}

#ifdef DEBUG
void Engine::rerunDebugMovePhy()
{
    const ecs::EntityId entityId0 = ecs.getEntityIdFromIdx(0);
    if (!ecs.validId(entityId0))
    {
        return;
    }
    entt::entity ent0 = ecs.getEntity(entityId0);
    if (ent0 == entt::null || !ptrHandle->registry->valid(ent0))
    {
        return;
    }

    auto& reg = *ptrHandle->registry;
    auto* tr = reg.try_get<ecs::Transform>(ent0);
    auto* pb = reg.try_get<ecs::PhysicsBody>(ent0);
    auto* th = reg.try_get<ecs::PhyThrust>(ent0);
    auto* moveCtrl = reg.try_get<ecs::MoveCtrl>(ent0);
    if (!tr || !pb)
    {
        return;
    }

    rerunStream.log("rot/vel", rerun::Scalars({pb->rotVel}));
    if (moveCtrl)
    {
        rerunStream.log("rot/des_vel", rerun::Scalars({moveCtrl->spRotVel}));
        rerunStream.log(
            "rot/vel_err",
            rerun::Scalars({pb->rotVel - moveCtrl->spRotVel}));
    }
    rerunStream.log("rot/trq", rerun::Scalars({th ? th->torque : 0.f}));
    rerunStream.log("pos/x", rerun::Scalars({tr->pos.x}));
    rerunStream.log("pos/y", rerun::Scalars({tr->pos.y}));
    if (moveCtrl)
    {
        rerunStream.log("pos/des/x", rerun::Scalars({moveCtrl->spPos.x}));
        rerunStream.log("pos/des/y", rerun::Scalars({moveCtrl->spPos.y}));
    }
    rerunStream.log("pos/vel/x", rerun::Scalars({pb->vel.x}));
    rerunStream.log("pos/vel/y", rerun::Scalars({pb->vel.y}));
    if (moveCtrl)
    {
        rerunStream.log("pos/des_vel/x", rerun::Scalars({moveCtrl->spVelX}));
        rerunStream.log("pos/des_vel/y", rerun::Scalars({moveCtrl->spVelY}));
        rerunStream.log(
            "pos/vel_err/x",
            rerun::Scalars({pb->vel.x - moveCtrl->spVelX}));
        rerunStream.log(
            "pos/vel_err/y",
            rerun::Scalars({pb->vel.y - moveCtrl->spVelY}));
    }
    rerunStream.log("pos/thrust/x",
                    rerun::Scalars({th ? th->thrustGlobal.x : 0.f}));
    rerunStream.log("pos/thrust/y",
                    rerun::Scalars({th ? th->thrustGlobal.y : 0.f}));
}
#endif

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
        [this](const cmd::CommandArgs&) { return "Ok: Pong"; },
        "Reply with a pong message");

    commandManager.registerCommand({"log", "warn"},
                                   [this](const cmd::CommandArgs& a)
                                   {
                                       LG_W("{}", a.flags.at("-m"));
                                       return "Ok";
                                   },
                                   "Log a warning on the server",
                                   {{"-m", "Message text", true}});

    commandManager.registerCommand({"log"},
                                   [this](const cmd::CommandArgs& a)
                                   {
                                       LG_I("{}", a.flags.at("-m"));
                                       return "Ok";
                                   },
                                   "Log an info message on the server",
                                   {{"-m", "Message text", true}});

    commandManager.registerCommand(
        {"help"},
        [this](const cmd::CommandArgs& a)
        {
            if (a.positionals.empty())
            {
                return commandManager.help("");
            }
            string path = a.positionals[0];
            for (size_t i = 1; i < a.positionals.size(); ++i)
            {
                path += "." + a.positionals[i];
            }
            return commandManager.help(path);
        },
        "List all commands, or: help <cmd> [subcmd ...] (e.g. help asset "
        "list)");

    commandManager.registerCommand(
        {"asset", "list"},
        [this](const cmd::CommandArgs& a)
        {
            string cat = "all";
            const auto it = a.flags.find("-c");
            if (it != a.flags.end() && !it->second.empty())
            {
                cat = it->second;
            }
            else if (!a.positionals.empty())
            {
                cat = a.positionals[0];
            }
            return assetFactory.assetList(cat);
        },
        "List assets, optionally filtered by category",
        {{"-c", "Category filter (default: all)", false}});

    commandManager.registerCommand(
        {"asset", "info"},
        [this](const cmd::CommandArgs& a)
        { return assetFactory.assetInfo(a.flags.at("-a")); },
        "Print information for one asset",
        {{"-a", "Asset id", true}});

    commandManager.registerCommand(
        {"move-ctrl", "set"},
        [this](const cmd::CommandArgs& a) -> std::string
        {
            try
            {
                std::string idxStr = a.flags.at("-e");
                uint32_t idx = std::stoul(idxStr);
                ecs::EntityId entityId = ecs.getEntityIdFromIdx(idx);

                entt::entity ent = ecs.getEntity(entityId);
                if (ent == entt::null)
                {
                    return "Failed: Entity not found";
                }

                if (ecs.getRegistry().all_of<ecs::MoveCtrl>(ent))
                {
                    auto& mc = ecs.getRegistry().get<ecs::MoveCtrl>(ent);

                    if (const auto it = a.flags.find("-x");
                        it != a.flags.end() && !it->second.empty())
                    {
                        mc.spPos.x = std::stof(it->second);
                    }
                    if (const auto it = a.flags.find("-y");
                        it != a.flags.end() && !it->second.empty())
                    {
                        mc.spPos.y = std::stof(it->second);
                    }
                    if (const auto it = a.flags.find("-r");
                        it != a.flags.end() && !it->second.empty())
                    {
                        mc.spRot = std::stof(it->second);
                    }

                    mc.active = true;
                }
                else
                {
                    return "Failed: MoveCtrl component not found";
                }

                return "Ok";
            }
            catch (const std::exception& e)
            {
                return std::string("Failed: ") + e.what();
            }
        },
        "Update move-ctrl setpoints for an entity",
        {{"-e", "Entity id", true},
         {"-x", "X position", false},
         {"-y", "Y position", false},
         {"-r", "Rotation", false}});

    commandManager.registerCommand(
        {"phy", "set"},
        [this](const cmd::CommandArgs& a) -> std::string
        {
            try
            {
                const uint32_t idx = std::stoul(a.flags.at("-e"));
                ecs::EntityId entityId = ecs.getEntityIdFromIdx(idx);
                entt::entity ent = ecs.getEntity(entityId);
                if (ent == entt::null)
                {
                    return "Failed: Entity not found";
                }
                auto* pb = ecs.getRegistry().try_get<ecs::PhysicsBody>(ent);
                if (!pb)
                {
                    return "Failed: PhysicsBody component not found";
                }
                const auto mIt = a.flags.find("-m");
                const auto iIt = a.flags.find("-i");
                const bool hasMass =
                    mIt != a.flags.end() && !mIt->second.empty();
                const bool hasInertia =
                    iIt != a.flags.end() && !iIt->second.empty();
                if (!hasMass && !hasInertia)
                {
                    return "Failed: Provide -m and/or -i";
                }
                if (hasMass)
                {
                    const float mass = std::stof(mIt->second);
                    if (!(mass > 0.0f))
                    {
                        return "Failed: mass must be > 0";
                    }
                    pb->mass = mass;
                }
                if (hasInertia)
                {
                    const float inertia = std::stof(iIt->second);
                    if (!(inertia > 0.0f))
                    {
                        return "Failed: inertia must be > 0";
                    }
                    pb->inertia = inertia;
                }
                return "Ok";
            }
            catch (const std::exception& e)
            {
                return std::string("Failed: ") + e.what();
            }
        },
        "Set physics body mass/inertia for one entity",
        {{"-e", "Entity id", true},
         {"-m", "Mass (>0)", false},
         {"-i", "Inertia (>0)", false}});

    commandManager.registerCommand(
        {"phy", "setk"},
        [this](const cmd::CommandArgs& a) -> std::string
        {
            try
            {
                const auto ktIt = a.flags.find("-kt");
                const auto krIt = a.flags.find("-kr");
                const bool hasKt =
                    ktIt != a.flags.end() && !ktIt->second.empty();
                const bool hasKr =
                    krIt != a.flags.end() && !krIt->second.empty();
                if (!hasKt && !hasKr)
                {
                    return "Failed: Provide -kt and/or -kr";
                }
                if (hasKt)
                {
                    const float kpThrust = std::stof(ktIt->second);
                    if (!(kpThrust >= 0.0f))
                    {
                        return "Failed: kp-thrust must be >= 0";
                    }
                    ptrHandle->kpThrust = kpThrust;
                }
                if (hasKr)
                {
                    const float kpTurn = std::stof(krIt->second);
                    if (!(kpTurn >= 0.0f))
                    {
                        return "Failed: kp-turn must be >= 0";
                    }
                    ptrHandle->kpTurn = kpTurn;
                }
                return "Ok";
            }
            catch (const std::exception& e)
            {
                return std::string("Failed: ") + e.what();
            }
        },
        "Set global physics controller gains (thrust/turn)",
        {{"-kt", "Thrust velocity P gain (>=0)", false},
         {"-kr", "Turn velocity P gain (>=0)", false}});
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