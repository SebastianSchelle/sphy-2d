#include "bitsery/serializer.h"
#include "ecs.hpp"
#include "rerun.hpp"
#include "sector.hpp"
#include "std-inc.hpp"
#include <comp-gfx.hpp>
#include <comp-ident.hpp>
#include <comp-phy.hpp>
#include <components/comp-phy.hpp>
#include <engine-impl.hpp>
#include <net-shared.hpp>
#include <protocol.hpp>
#include <random>
#include <server.hpp>
#include <sys-phy.hpp>
#include <thread>
#include <version.hpp>
#include <work-distributor.hpp>

namespace sphys
{

Engine::Engine(const sphy::CmdLinOptionsServer& options,
               cfg::ConfigManager& config)
    : options(options), config(config), state(EngineState::Init), saveConfig(),
      saveFolder(options.savedir), rerunStream("sphy-2d-move-ctrl"),
      commandManager()
{
    ptrHandle = new ecs::PtrHandle();
    ptrHandle->ecs = &ecs;
    ptrHandle->registry = &ecs.getRegistry();
    ptrHandle->engine = this;
    ptrHandle->world = &world;
    ptrHandle->engine = this;
    ptrHandle->systems = &ecs.getRegisteredSystems();
    ptrHandle->registry = &ecs.getRegistry();
    ptrHandle->workDistributor = &workDistributor;
    ptrHandle->kpThrust =
        CFG_FLOAT(config, 25.0f, "engine", "physics", "kp-thrust");
    ptrHandle->kpTurn =
        CFG_FLOAT(config, 25.0f, "engine", "physics", "kp-turn");
    ptrHandle->angDrag =
        CFG_FLOAT(config, 0.1f, "engine", "physics", "ang-drag");
    ptrHandle->linDrag =
        CFG_FLOAT(config, 0.1f, "engine", "physics", "lin-drag");
    ptrHandle->minFaceTargetDist =
        CFG_FLOAT(config, 1.0f, "engine", "physics", "min-face-target-dist");
    slowDumpUs =
        1000 * CFG_UINT(config, 1000.0f, "engine", "upd", "dump-int", "slow");
    activeSectorDumpUs =
        1000
        * CFG_UINT(config, 30.0f, "engine", "upd", "dump-int", "active-sector");
    int updThreads = CFG_UINT(config, 2.0f, "engine", "upd", "threads");

    updThreads = std::clamp(updThreads, 1, 16);
    workDistributor.init(updThreads);

    if (options.enableRerun)
    {
        rerunStream.spawn().exit_on_failure();
    }
}

Engine::~Engine()
{
    stopRequested = true;
    /*if (engineThread.joinable())
    {
        engineThread.join();
    }*/
}

void Engine::start()
{
    auto cFac = &assetFactory.componentFactory;
    cFac->registerComponent<ecs::Transform>();
    cFac->registerComponent<ecs::PhysicsBody>();
    cFac->registerComponent<ecs::AssetId>();
    cFac->registerComponent<ecs::PhyThrust>();
    cFac->registerComponent<ecs::MoveCtrl>();
    cFac->registerComponent<ecs::Collider>();
    cFac->registerComponent<ecs::Broadphase>();
    cFac->registerComponent<ecs::TransformCache>();
    cFac->registerComponent<ecs::MapIcon>();
    cFac->registerComponent<ecs::Textures>();

    ecs.registerSystem(ecs::sysMoveCtrl);
    ecs.registerSystem(ecs::sysPhyThrust);
    ecs.registerSystem(ecs::sysPhysics);
    ecs.registerSystem(ecs::sysCollisionDetection);

    registerConsoleCommands();

    registerSlowDumpComponent<ecs::Transform>();
    registerActiveSectorDumpComponent<ecs::Transform>();

    def::ClientInfoHandle handle = registerClient(def::ClientInfo(
        "Based Laser King",
        net::ClientInfo{
            .token = "1234abcd1234abcd",
            .portUdp = 0,
            .address = asio::ip::address::from_string("0.0.0.0"),
        },
        def::ClientFlags{.enConsole = 1}));
    auto clientInfo = clientLib.getItem(handle);
    clientInfo->setActiveEntity(ecs::EntityId{0, 1});

    // engineThread = std::thread([this]() { engineLoop(); });
    engineLoop();
}

void Engine::stop()
{
    stopRequested = true;
    /*if (engineThread.joinable())
    {
        engineThread.join();
    }*/
}

void Engine::engineLoop()
{
    long lastSaveTime = tim::nowU();
    long lastUpdateTime = tim::nowU();
    long lastFpsUpdate = tim::nowU();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    while (!stopRequested)
    {
        long nowU = tim::nowU();
        float dt = (nowU - lastUpdateTime) / 1000000.0f;
        if (dt < 1e-6f)
        {
            dt = 1e-6f;
        }
        filteredFps = 0.9f * filteredFps + 0.1f * (1.0f / dt);

        DO_PERIODIC_U_EXTNOW(lastFpsUpdate,
                             5000000,
                             nowU,
                             [this]() { LG_I("FPS: {}", filteredFps); });

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
                    testSpawn();
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
                    testSpawn();
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
                runConnectedClientWorkSequencers();
                runActiveSectorDump(nowU);
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
            // system.function(entity, entityId, dt, ptrHandle);
        }
    }
    world.update(dt, ptrHandle);
    markPlayerSectors();
    rerunDebugMovePhy();
}

void Engine::runConnectedClientWorkSequencers()
{
    for (auto handle : connectedClientHandles)
    {
        clientLib.getItem(handle)->executeWorkSequencer();
    }
}

void Engine::rerunDebugMovePhy()
{
    if (!options.enableRerun)
    {
        return;
    }
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

    if (moveCtrl && tr && pb && th)
    {
        rerunStream.log("rot/vel", rerun::Scalars({pb->rotVel}));
        rerunStream.log("rot/trq", rerun::Scalars({th ? th->torque : 0.f}));
        rerunStream.log("pos/x", rerun::Scalars({tr->pos.x}));
        rerunStream.log("pos/y", rerun::Scalars({tr->pos.y}));
        rerunStream.log("pos/des/x",
                        rerun::Scalars({moveCtrl->spPos.sectorPos.x}));
        rerunStream.log("pos/des/y",
                        rerun::Scalars({moveCtrl->spPos.sectorPos.y}));
        rerunStream.log("pos/vel/x", rerun::Scalars({pb->vel.x}));
        rerunStream.log("pos/vel/y", rerun::Scalars({pb->vel.y}));
        rerunStream.log("pos/thrust/x",
                        rerun::Scalars({th ? th->thrustGlobal.x : 0.f}));
        rerunStream.log("pos/thrust/y",
                        rerun::Scalars({th ? th->thrustGlobal.y : 0.f}));
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

def::ClientInfoHandle Engine::registerClient(const def::ClientInfo& clientInfo)
{
    return clientLib.addItem(clientInfo.clientInfo.token, clientInfo);
}

void Engine::parseCommandData(const net::CmdQueueData& cmdData)
{
    if (cmdData.sendType == net::SendType::TCP && cmdData.tcpDisconnected)
    {
        handleTcpDisconnect(
            cmdData.tcpConnection,
            def::ClientInfoHandle(cmdData.tcpDisconnectedHandleValue));
        return;
    }

    try
    {
        std::string token;
        const udp::endpoint* udpEndpoint;
        net::TcpConnection* tcpConnection = nullptr;
        def::ClientInfoHandle clientInfoHandle =
            def::ClientInfoHandle::Invalid();
        def::ClientInfo* clientInfo = nullptr;

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
            def::ClientInfoHandle handle = clientLib.getHandle(token);
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
            clientInfoHandle = *(
                (def::ClientInfoHandle*)&tcpConnection->getClientInfoHandle());
            if (clientInfoHandle.isValid())
            {
                clientInfo = clientLib.getItem(clientInfoHandle);
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

            if (dataStartPos + len > data.size())
            {
                LG_W("Command data too short cmd={}, len={}", cmd, len);
                break;
            }
            parseCommand(cmddes,
                         token,
                         udpEndpoint,
                         tcpConnection,
                         clientInfoHandle,
                         clientInfo,
                         cmdData.sendType,
                         cmd,
                         flags,
                         len);
            size_t readPos = cmddes.adapter().currentReadPos();
            if (readPos - dataStartPos != len)
            {
                LG_W("Command data length mismatch. Expected: {}, Read: {}",
                     len,
                     cmddes.adapter().currentReadPos() - dataStartPos);
                return;
            }
            // cmddes.adapter().currentReadPos(dataStartPos + len);
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
                          net::TcpConnection* tcpConnection,
                          def::ClientInfoHandle clientInfoHandle,
                          def::ClientInfo* clientInfo,
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
                    LG_I("UDP Log from {}: {}", clientInfo->getName(), str);
                }
                else if (sendType == net::SendType::TCP)
                {
                    LG_I("TCP Log from {}: {}", clientInfo->getName(), str);
                }
            }
            break;
        }
        case prot::cmd::TIME_SYNC:
        {
            if (sendType == net::SendType::UDP && (flags & CMD_FLAG_RESP) == 0)
            {
                long d = tim::nowU();
                prot::MsgComposer mcomp(net::SendType::UDP, *udpEndpoint);
                mcomp.startCommand(prot::cmd::TIME_SYNC, CMD_FLAG_RESP);
                mcomp.ser->value8b(d);
                mcomp.execute(sendQueue);
            }
            break;
        }
        case prot::cmd::VERSION_CHECK:
        {
            if (sendType == net::SendType::TCP && (flags & CMD_FLAG_RESP) == 0)
            {
                uint16_t major;
                uint16_t minor;
                uint16_t patch;
                cmddes.value2b(major);
                cmddes.value2b(minor);
                cmddes.value2b(patch);
                prot::MsgComposer mcomp(net::SendType::TCP, tcpConnection);
                mcomp.startCommand(prot::cmd::VERSION_CHECK, CMD_FLAG_RESP);
                mcomp.ser->value2b(version::MAJOR);
                mcomp.ser->value2b(version::MINOR);
                mcomp.ser->value2b(version::PATCH);
                mcomp.execute(sendQueue);
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
                    def::ClientInfoHandle handle = clientLib.getHandle(token);
                    if (handle.isValid())
                    {
                        auto address =
                            tcpConnection->socket().local_endpoint().address();
                        def::ClientInfo* clientInfo = clientLib.getItem(handle);
                        clientInfo->clientInfo.portUdp = portUdp;
                        clientInfo->clientInfo.address = address;
                        clientInfo->clientInfo.udpEndpoint =
                            udp::endpoint(address, portUdp);
                        clientInfo->clientInfo.connection = tcpConnection;
                        clientInfo->clientInfo.connection->setClientInfoHandle(
                            *((net::TcpClientInfoHandle*)&handle));
                        connectedClientHandles.push_back(handle);
                        LG_I(
                            "Client authenticated. ip={}, udp port={}, "
                            "token={}",
                            address.to_string(),
                            portUdp,
                            token);
                        prot::MsgComposer mcomp(net::SendType::TCP,
                                                tcpConnection);
                        mcomp.startCommand(prot::cmd::AUTHENTICATE,
                                           CMD_FLAG_RESP);
                        mcomp.startCommand(prot::cmd::ACTIVE_ENTITY_SWITCHED,
                                           0);
                        mcomp.ser->object(clientInfo->getActiveEntity());
                        mcomp.execute(sendQueue);
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
                prot::MsgComposer mcomp(net::SendType::TCP, tcpConnection);
                mcomp.startCommand(prot::cmd::WORLD_INFO, CMD_FLAG_RESP);
                auto worldShape = world.getWorldShape();
                mcomp.ser->object(worldShape);
                mcomp.execute(sendQueue);
            }
            break;
        }
        case prot::cmd::NOTIFY_CLIENT_READY:
        {
            if (sendType == net::SendType::TCP && (flags & CMD_FLAG_RESP) == 0)
            {
                LG_I("Server accepted client readyness of {}",
                     clientInfo->getName());
                activeClientHandles.push_back(clientInfoHandle);
                prot::MsgComposer mcomp(net::SendType::TCP, tcpConnection);
                mcomp.startCommand(prot::cmd::NOTIFY_CLIENT_READY,
                                   CMD_FLAG_RESP);
                mcomp.execute(sendQueue);
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
                if (clientInfo)
                {
                    if (clientInfo->getFlags().enConsole)
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
                        response = "Failed: Client " + clientInfo->getName()
                                   + " is not a console";
                    }
                }
                else
                {
                    response = "Failed: Client info not found";
                }
                prot::MsgComposer mcomp(net::SendType::TCP, tcpConnection);
                mcomp.startCommand(prot::cmd::CONSOLE_CMD, CMD_FLAG_RESP);
                mcomp.ser->text1b(response, response.size());
                mcomp.execute(sendQueue);
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
            break;
        }
        case prot::cmd::ENT_CMD_MOVETO_POS:
        {
            if (sendType == net::SendType::TCP && (flags & CMD_FLAG_RESP) == 0)
            {
                ecs::EntityId entityId;
                def::SectorCoords sectorCoords;
                cmddes.object(entityId);
                cmddes.object(sectorCoords);
                entt::entity ent = ecs.getEntity(entityId);
                if (ent == entt::null)
                {
                    // todo: error handling?
                    return;
                }
                // todo: check if allowed
                auto* moveCtrl =
                    ptrHandle->registry->try_get<ecs::MoveCtrl>(ent);
                if (moveCtrl)
                {
                    moveCtrl->active = true;
                    moveCtrl->spPos = sectorCoords;
                    moveCtrl->faceDirMode = ecs::MoveCtrl::FaceDirMode::Forward;
                }
            }
            break;
        }
        case prot::cmd::DBG_GET_AABB_TREE:
        {
            if ((flags & CMD_FLAG_RESP) == 0)
            {
                uint32_t sectorId;
                cmddes.value4b(sectorId);
                handleGetAabbTree(sectorId, tcpConnection);
            }
            break;
        }
        case prot::cmd::ALL_ENTT_COMPONENTS:
        {
            if ((flags & CMD_FLAG_RESP) == 0)
            {
                // todo: threaded or max number of entities per frame, queue
                // clients to send to
                LG_D("Sending all entt components to client {}",
                     clientInfo->getName());
                sendAllEnttComponents(clientInfo, tcpConnection);
                prot::MsgComposer mcomp(net::SendType::TCP, tcpConnection);
                mcomp.startCommand(prot::cmd::ALL_ENTT_COMPONENTS,
                                   CMD_FLAG_RESP);
                mcomp.execute(sendQueue);
            }
            break;
        }
        default:
            break;
    }
}

ecs::EntityId Engine::spawnEntityFromAsset(const std::string& assetId)
{
    return ecs.spawnEntityFromAsset(assetId, assetFactory);
}

ecs::EntityId Engine::spawnEntityFromAsset(const std::string& assetId,
                                           uint32_t sectorId,
                                           const ecs::Transform& transform)
{
    ecs::EntityId ent = spawnEntityFromAsset(assetId);
    if (!ecs.validId(ent))
    {
        LG_E("Failed to spawn entity. Asset with id {} does not exist",
             assetId);
        return ent;
    }
    entt::entity entity = ecs.getEntity(ent);
    auto& reg = ecs.getRegistry();
    ecs::Transform& tr = reg.get_or_emplace<ecs::Transform>(entity);
    ecs::Broadphase& br = reg.get_or_emplace<ecs::Broadphase>(entity);
    ecs::TransformCache& trC = reg.get_or_emplace<ecs::TransformCache>(entity);
    ecs::SectorId& sec = reg.get_or_emplace<ecs::SectorId>(
        entity, ecs::SectorId{world::INVALID_SECTOR_ID, 0, 0});
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
                    auto* sectorId =
                        ecs.getRegistry().try_get<ecs::SectorId>(ent);

                    const auto sxIt = a.flags.find("-sx");
                    const auto syIt = a.flags.find("-sy");
                    const bool hasSx =
                        sxIt != a.flags.end() && !sxIt->second.empty();
                    const bool hasSy =
                        syIt != a.flags.end() && !syIt->second.empty();
                    if (hasSx != hasSy)
                    {
                        return "Failed: provide both -sx and -sy";
                    }
                    if (hasSx)
                    {
                        mc.spPos.pos.x = std::stoul(sxIt->second);
                        mc.spPos.pos.y = std::stoul(syIt->second);
                    }
                    else if (!mc.active && sectorId)
                    {
                        // On first activation, default target sector to
                        // current.
                        auto [sx, sy] =
                            ptrHandle->world->idToSectorCoords(sectorId->id);
                        mc.spPos.pos.x = sx;
                        mc.spPos.pos.y = sy;
                    }

                    if (const auto it = a.flags.find("-x");
                        it != a.flags.end() && !it->second.empty())
                    {
                        mc.spPos.sectorPos.x = std::stof(it->second);
                    }
                    if (const auto it = a.flags.find("-y");
                        it != a.flags.end() && !it->second.empty())
                    {
                        mc.spPos.sectorPos.y = std::stof(it->second);
                    }
                    if (const auto it = a.flags.find("-r");
                        it != a.flags.end() && !it->second.empty())
                    {
                        mc.spRot = std::stof(it->second);
                    }
                    if (const auto it = a.flags.find("-fd");
                        it != a.flags.end() && !it->second.empty())
                    {
                        string mode = it->second;
                        std::transform(mode.begin(),
                                       mode.end(),
                                       mode.begin(),
                                       [](unsigned char c)
                                       { return std::tolower(c); });
                        if (mode == "none")
                        {
                            mc.faceDirMode = ecs::MoveCtrl::FaceDirMode::None;
                        }
                        else if (mode == "forward")
                        {
                            mc.faceDirMode =
                                ecs::MoveCtrl::FaceDirMode::Forward;
                        }
                        else if (mode == "targetpoint")
                        {
                            mc.faceDirMode =
                                ecs::MoveCtrl::FaceDirMode::TargetPoint;
                        }
                        else
                        {
                            return "Failed: -fd must be "
                                   "none|forward|targetpoint";
                        }
                    }
                    if (const auto it = a.flags.find("-tx");
                        it != a.flags.end() && !it->second.empty())
                    {
                        mc.lookAt.x = std::stof(it->second);
                    }
                    if (const auto it = a.flags.find("-ty");
                        it != a.flags.end() && !it->second.empty())
                    {
                        mc.lookAt.y = std::stof(it->second);
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
         {"-sx", "Target sector X (must be used with -sy)", false},
         {"-sy", "Target sector Y (must be used with -sx)", false},
         {"-x", "Target in-sector X position", false},
         {"-y", "Target in-sector Y position", false},
         {"-r", "Target rotation (rad)", false},
         {"-fd", "Face direction mode: none|forward|targetpoint", false},
         {"-tx", "Target-point X (for -fd targetpoint)", false},
         {"-ty", "Target-point Y (for -fd targetpoint)", false}});

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
        def::ClientInfoHandle handle = activeClientHandles[i];
        def::ClientInfo* clientInfo = clientLib.getItem(handle);
        DO_PERIODIC_U_EXTNOW(clientInfo->lastSlowDump,
                             slowDumpUs,
                             frameTime,
                             [&]()
                             {
                                 for (auto& component : slowDumpComponents)
                                 {
                                     component.function(&clientInfo->clientInfo,
                                                        ptrHandle);
                                 }
                             });
    }
}

void Engine::runActiveSectorDump(long frameTime)
{
    for (int i = 0; i < activeClientHandles.size(); i++)
    {
        def::ClientInfoHandle handle = activeClientHandles[i];
        def::ClientInfo* clientInfo = clientLib.getItem(handle);
        DO_PERIODIC_U_EXTNOW(
            clientInfo->lastActiveSectorDump,
            activeSectorDumpUs,
            frameTime,
            [&]()
            {
                for (auto& sectorId : clientInfo->getActiveSectors())
                {
                    for (auto& component : activeSectorUpdates)
                    {
                        component.function(
                            &clientInfo->clientInfo, sectorId, ptrHandle);
                    }
                }
            });
    }
}

void Engine::handleTcpDisconnect(net::TcpConnection* conn,
                                 def::ClientInfoHandle disconnectedHandle)
{
    def::ClientInfoHandle handle = disconnectedHandle;
    if (conn)
    {
        if (!handle.isValid())
        {
            handle = *((def::ClientInfoHandle*)&conn->getClientInfoHandle());
        }
        conn->setClientInfoHandle(net::TcpClientInfoHandle{0, 0});
    }
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
    for (auto it = connectedClientHandles.begin();
         it != connectedClientHandles.end();)
    {
        if (it->value() == hv)
            it = connectedClientHandles.erase(it);
        else
            ++it;
    }
    if (def::ClientInfo* ci = clientLib.getItem(handle))
        ci->clientInfo.connection = nullptr;
    LG_I("TCP client disconnected (handle value={})", hv);
}

void Engine::sendAllEnttComponents(def::ClientInfo* clientInfo,
                                   net::TcpConnection* conn)
{
    ecs.iterateEntities(
        [this, clientInfo, conn](ecs::EntityId entityId)
        {
            const ecs::EntityId entityCopy = entityId;
            clientInfo->addWorkFunction(
                [this, entityCopy, conn]()
                { sendAllComponents(entityCopy, conn); });
        });
}

void Engine::sendAllComponents(ecs::EntityId entityId, net::TcpConnection* conn)
{
    entt::entity ent = ecs.getEntity(entityId);
    if (ent == entt::null)
    {
        // todo: reply needed in error case?
        return;
    }
    // todo: prevent exceeding tcp packet size
    prot::MsgComposer mcomp(net::SendType::TCP, conn);
    mcomp.startCommand(prot::cmd::REQ_ALL_COMPONENTS, CMD_FLAG_RESP);
    mcomp.ser->object(entityId);
    auto& reg = ecs.getRegistry();
    uint16_t numComponents = 0;
    for (auto& [hash, helper] :
         assetFactory.componentFactory.getComponentHelpers())
    {
        helper.serializeFromRegistry(reg, ent, *mcomp.ser);
        numComponents++;
        if (mcomp.ser->adapter().currentWritePos()
            >= prot::kMaxSerializedChunkBytes - 100)
        {
            mcomp.execute(sendQueue);
            numComponents = 0;
            mcomp.resetData();
            mcomp.startCommand(prot::cmd::REQ_ALL_COMPONENTS, CMD_FLAG_RESP);
            mcomp.ser->object(entityId);
        }
    }
    if (numComponents > 0)
    {
        mcomp.execute(sendQueue);
    }
}

void Engine::testSpawn()
{
    static constexpr const char* kAssets[] = {
        "test1", "test2", "test3", "test4"};
    static constexpr float kTwoPi = 6.2831855f;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> posDist(
        -world.getWorldShape().sectorSize / 2,
        world.getWorldShape().sectorSize / 2);
    std::uniform_real_distribution<float> rotDist(0.0f, kTwoPi);
    std::uniform_int_distribution<int> assetPick(0, 3);
    std::uniform_int_distribution<int> sectorPick(0,
                                                  world.getSectorCount() - 1);

    for (int i = 0; i < 50000; ++i)
    {
        auto ent = spawnEntityFromAsset(
            // kAssets[assetPick(gen)],
            "BoomBoa",
            sectorPick(gen),
            ecs::Transform{glm::vec2{posDist(gen), posDist(gen)},
                           rotDist(gen)});
        entt::entity entt = ecs.getEntity(ent);
        auto& reg = ecs.getRegistry();
        auto* moveCtrl = reg.try_get<ecs::MoveCtrl>(entt);
        if (moveCtrl)
        {
            moveCtrl->active = true;
            moveCtrl->spPos.sectorPos.x = posDist(gen);
            moveCtrl->spPos.sectorPos.y = posDist(gen);
            moveCtrl->spPos.pos = world.idToSectorCoords(sectorPick(gen));
            moveCtrl->faceDirMode = ecs::MoveCtrl::FaceDirMode::Forward;
        }
    }
}

void Engine::handleGetAabbTree(uint32_t sectorId, net::TcpConnection* conn)
{
    prot::MsgComposer mcomp(net::SendType::TCP, conn);
    mcomp.startCommand(prot::cmd::DBG_GET_AABB_TREE, CMD_FLAG_RESP);
    mcomp.ser->value4b(sectorId);
    std::vector<con::AABB> aabbs;
    auto* sector = world.getSector(sectorId);
    if (sector)
    {
        sector->getAllAABBs(aabbs);
    }
    else
    {
        LG_W("DBG_GET_AABB_TREE: sector {} not found", sectorId);
    }
    mcomp.ser->object(aabbs);
    mcomp.execute(sendQueue);
}

void Engine::markPlayerSectors()
{
    std::set<uint32_t> playerSectors;
    for (auto& clientHandle : activeClientHandles)
    {
        def::ClientInfo* clientInfo = clientLib.getItem(clientHandle);
        clientInfo->clearActiveSectors();
        if (clientInfo)
        {
            ecs::EntityId entityId = clientInfo->getActiveEntity();
            if (entityId != ecs::EntityId::Invalid())
            {
                entt::entity entity = ecs.getEntity(entityId);
                if (entity != entt::null)
                {
                    const float sectorSize = world.getWorldShape().sectorSize;
                    auto& reg = ecs.getRegistry();
                    ecs::SectorId* sectorId =
                        reg.try_get<ecs::SectorId>(entity);
                    ecs::Transform* transform =
                        reg.try_get<ecs::Transform>(entity);
                    if (sectorId && transform)
                    {
                        clientInfo->addActiveSector(sectorId->id);
                        // todo: what threshold for neighboring sectors?
                        def::SectorPos neighbor;
                        const bool north =
                            transform->pos.y < -sectorSize / 2 * 0.7;
                        const bool south =
                            transform->pos.y > sectorSize / 2 * 0.7;
                        const bool west =
                            transform->pos.x < -sectorSize / 2 * 0.7;
                        const bool east =
                            transform->pos.x > sectorSize / 2 * 0.7;
                        if (west
                            && world.getNeighboringSectorPos(
                                sectorId->id, def::Direction::W, neighbor))
                        {
                            clientInfo->addActiveSector(
                                world.sectorCoordsToId(neighbor.x, neighbor.y));
                            if (north
                                && world.getNeighboringSectorPos(
                                    sectorId->id, def::Direction::NW, neighbor))
                            {
                                clientInfo->addActiveSector(
                                    world.sectorCoordsToId(neighbor.x,
                                                           neighbor.y));
                            }
                            else if (south
                                     && world.getNeighboringSectorPos(
                                         sectorId->id,
                                         def::Direction::SW,
                                         neighbor))
                            {
                                clientInfo->addActiveSector(
                                    world.sectorCoordsToId(neighbor.x,
                                                           neighbor.y));
                            }
                        }
                        else if (east
                                 && world.getNeighboringSectorPos(
                                     sectorId->id, def::Direction::E, neighbor))
                        {
                            clientInfo->addActiveSector(
                                world.sectorCoordsToId(neighbor.x, neighbor.y));
                            if (north
                                && world.getNeighboringSectorPos(
                                    sectorId->id, def::Direction::NE, neighbor))
                            {
                                clientInfo->addActiveSector(
                                    world.sectorCoordsToId(neighbor.x,
                                                           neighbor.y));
                            }
                            else if (south
                                     && world.getNeighboringSectorPos(
                                         sectorId->id,
                                         def::Direction::SE,
                                         neighbor))
                            {
                                clientInfo->addActiveSector(
                                    world.sectorCoordsToId(neighbor.x,
                                                           neighbor.y));
                            }
                        }
                        if (north
                            && world.getNeighboringSectorPos(
                                sectorId->id, def::Direction::N, neighbor))
                        {
                            clientInfo->addActiveSector(
                                world.sectorCoordsToId(neighbor.x, neighbor.y));
                        }
                        else if (south
                                 && world.getNeighboringSectorPos(
                                     sectorId->id, def::Direction::S, neighbor))
                        {
                            clientInfo->addActiveSector(
                                world.sectorCoordsToId(neighbor.x, neighbor.y));
                        }
                    }
                }
            }
        }
        playerSectors.insert(clientInfo->getActiveSectors().begin(),
                             clientInfo->getActiveSectors().end());
    }
    world.markPlayerSectors(playerSectors);
}

}  // namespace sphys

template class con::ItemLib<net::ClientInfo>;