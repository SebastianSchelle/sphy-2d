#include "bitsery/serializer.h"
#include "comp-ai.hpp"
#include "ecs.hpp"
#include "lib-station-part.hpp"
#include "sector.hpp"
#include "std-inc.hpp"
#include "task-basic.hpp"
#include "task-system.hpp"
#include <comp-gfx.hpp>
#include <comp-ident.hpp>
#include <comp-phy.hpp>
#include <comp-struct.hpp>
#include <comp-tag.hpp>
#include <comp-turret.hpp>
#include <components/comp-phy.hpp>
#include <engine-impl.hpp>
#include <net-shared.hpp>
#include <protocol.hpp>
#include <random>
#include <server.hpp>
#include <sys-ai.hpp>
#include <sys-lifetime.hpp>
#include <sys-phy.hpp>
#include <sys-turret.hpp>
#include <thread>
#include <version.hpp>
#include <work-distributor.hpp>

namespace sphys
{

Engine::Engine(const sphy::CmdLinOptionsServer& options,
               cfg::ConfigManager& config)
    : options(options), config(config), state(EngineState::Init), saveConfig(),
      saveFolder(options.savedir), commandManager()
{
    ptrHandle = new ecs::PtrHandle();
    ptrHandle->ecs = &ecs;
    ptrHandle->registry = &ecs.getRegistry();
    ptrHandle->engine = this;
    ptrHandle->taskSystem = &taskSystem;
    ptrHandle->world = &world;
    ptrHandle->engine = this;
    ptrHandle->systems = &ecs.getRegisteredSystems();
    ptrHandle->registry = &ecs.getRegistry();
    ptrHandle->workDistributor = &workDistributor;
    ptrHandle->colliderLib = &modManager.getColliderLib();
    ptrHandle->modManager = &modManager;
    ptrHandle->frameCnt = 0;
    ptrHandle->collisionLayerMat = &collisionLayerMat;
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
    ptrHandle->miningRate =
        CFG_FLOAT(config, 0.01f, "engine", "mining", "mining-rate");
    int updThreads = CFG_UINT(config, 2.0f, "engine", "upd", "threads");

    itemLifetime =
        CFG_FLOAT(config, 600.0f, "engine", "items", "item-lifetime");

    updThreads = std::clamp(updThreads, 1, 16);
    workDistributor.init(updThreads);

    entitySpawner = std::make_unique<EntitySpawner>(
        ecs, modManager, world, ptrHandle, taskSystem);
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
    assetFactory.componentFactory.registerAllComponents();

    ecs.registerSystem(ecs::sysLifetime);
    ecs.registerSystem(ecs::sysMoveCtrl);
    ecs.registerSystem(ecs::sysPhyThrust);
    ecs.registerSystem(ecs::sysPhysics);
    ecs.registerSystem(ecs::sysCollisionDetection);
    ecs.registerSystem(ecs::sysAnchorFixed);
    ecs.registerSystem(ecs::sysAi);
    ecs.registerSystem(ecs::sysTurret);

    loadCollisionMatrix();
    registerConsoleCommands();

    registerSlowDumpComponent<ecs::Transform>();
    registerActiveSectorDumpComponent<ecs::Transform>();
    registerActiveSectorDumpComponent<ecs::Turret>();

    def::ClientInfoHandle handle = registerClient(
        def::ClientInfo("Based Laser King",
                        net::ClientInfo{
                            .token = "1234abcd1234abcd",
                            .portUdp = 0,
                            .address = asio::ip::make_address("0.0.0.0"),
                        },
                        CLIENT_FLAG_EN_CONSOLE));
    auto clientInfo = clientLib.getItem(handle);
    clientInfo->activeEntity = ecs::EntityId{0, 1};

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
                    initPost();
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
        // std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (state == EngineState::Running || state == EngineState::Paused)
    {
        LG_I("Shutdown requested, saving game...");
        saveGame();
    }
}

void Engine::initPost()
{
    EntitySpawner::ItemSpawnConfig itemConfig;
    itemConfig.colliderHandle = modManager.getColliderLib().getHandle("Item");
    itemConfig.lifetime = itemLifetime;
    entitySpawner->setItemSpawnConfig(itemConfig);
}

void Engine::update(float dt)
{
    ptrHandle->frameCnt++;
    for (int i = 0; i < globalEntityIds.size(); i++)
    {
        ecs::EntityId entityId = globalEntityIds[i];
        entt::entity entity = globalEntities[i];
        for (auto system : *ptrHandle->systems)
        {
            // system.function(entity, entityId, dt, ptrHandle);
        }
    }
    // Update third person controlled vehicles
    for (auto handle : connectedClientHandles)
    {
        def::ClientInfo* clientInfo = clientLib.getItem(handle);
        auto& thrdCtrl = clientInfo->thirdPersonControl;
        if (thrdCtrl.flags
            & (def::ThirdPersonControl::FLG_DRIVE_MANUAL
               | def::ThirdPersonControl::FLG_TURN_MANUAL
               | def::ThirdPersonControl::FLG_FIRE_WEAPONS))
        {
            ecs::EntityId entityId = clientInfo->activeEntity;
            entt::entity ent = ecs.getEntity(entityId);
            if (ent == entt::null)
            {
                LG_W("Entity not found for client {}", clientInfo->name);
                continue;
            }
            auto* transform = ptrHandle->registry->try_get<ecs::Transform>(ent);
            auto* phyThrust = ptrHandle->registry->try_get<ecs::PhyThrust>(ent);
            auto* moveCtrl = ptrHandle->registry->try_get<ecs::MoveCtrl>(ent);
            if (phyThrust && transform && moveCtrl)
            {
                if (thrdCtrl.thrust == vec2(0.0f, 0.0f))
                {
                    moveCtrl->moveMode = ecs::MoveCtrl::MoveMode::Brake;
                }
                else
                {
                    if (thrdCtrl.thrust.x == 0.0f)
                    {
                        moveCtrl->moveMode =
                            ecs::MoveCtrl::MoveMode::BrakeManeuver;
                    }
                    else if (thrdCtrl.thrust.y == 0.0f)
                    {
                        moveCtrl->moveMode = ecs::MoveCtrl::MoveMode::BrakeMain;
                    }
                    else
                    {
                        moveCtrl->moveMode = ecs::MoveCtrl::MoveMode::None;
                    }
                    // Local x = maneuver (strafe), y = main (forward); use
                    // Transform::rot — TransformCache may lag when rotVel is
                    // small.
                    phyThrust->setThrustLocal(
                        thrdCtrl.thrust
                            * vec2(phyThrust->thrustManeuverMax,
                                   phyThrust->thrustMainMax),
                        *transform);
                }
                if (thrdCtrl.torque == 0.0f)
                {
                    moveCtrl->turnMode = ecs::MoveCtrl::TurnMode::Brake;
                }
                else
                {
                    moveCtrl->turnMode = ecs::MoveCtrl::TurnMode::None;
                    phyThrust->setTorque(thrdCtrl.torque
                                         * phyThrust->maxTorque);
                }
            }
            else
            {
                LG_W(
                    "PhyThrust, Transform, or MoveCtrl not found for client {}",
                    clientInfo->name);
                continue;
            }
        }
    }
    world.update(dt, ptrHandle);
    markPlayerSectors();
}

void Engine::runConnectedClientWorkSequencers()
{
    for (auto handle : connectedClientHandles)
    {
        clientLib.getItem(handle)->executeWorkSequencer();
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
    mod::PtrHandles ptrHandles{.assetFactory = &assetFactory};
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

        while (cmddes.adapter().currentReadPos()
               <= data.size() - prot::kCommandHeaderSize)
        {
            uint16_t cmd;
            uint8_t flags;
            uint32_t len;
            cmddes.value2b(cmd);
            cmddes.value1b(flags);
            cmddes.value4b(len);
            size_t dataStartPos = cmddes.adapter().currentReadPos();

            if (len > prot::kMaxCommandPayloadBytes)
            {
                LG_E("Command payload length too large: cmd={}, len={}",
                     cmd,
                     len);
                break;
            }
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
                         dataStartPos + len);
            size_t readPos = cmddes.adapter().currentReadPos();
            if (readPos - dataStartPos != len)
            {
                LG_W(
                    "Command data length mismatch: Cmd: {}, Flags: {}, "
                    "Expected: {}, Read: {}",
                    cmd,
                    flags,
                    len,
                    cmddes.adapter().currentReadPos() - dataStartPos);
            }
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
                          net::TcpConnection* tcpConnection,
                          def::ClientInfoHandle clientInfoHandle,
                          def::ClientInfo* clientInfo,
                          net::SendType sendType,
                          uint16_t cmd,
                          uint8_t flags,
                          size_t dataEndPos)
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
                        def::ClientInfo* clientInfo = clientLib.getItem(handle);
                        if (clientInfo->clientInfo.connection != nullptr
                            && clientInfo->clientInfo.connection
                                   != tcpConnection)
                        {
                            clientInfo->clientInfo.connection->close();
                        }
                        clientInfo->clearWorkSequencer();
                        auto address =
                            tcpConnection->socket().remote_endpoint().address();
                        clientInfo->clientInfo.portUdp = portUdp;
                        clientInfo->clientInfo.address = address;
                        clientInfo->clientInfo.udpEndpoint =
                            udp::endpoint(address, portUdp);
                        clientInfo->clientInfo.connection = tcpConnection;
                        clientInfo->clientInfo.connection->setClientInfoHandle(
                            *((net::TcpClientInfoHandle*)&handle));
                        bool alreadyConnected = false;
                        for (const auto& connected : connectedClientHandles)
                        {
                            if (connected.value() == handle.value())
                            {
                                alreadyConnected = true;
                                break;
                            }
                        }
                        if (!alreadyConnected)
                        {
                            connectedClientHandles.push_back(handle);
                        }
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
                        mcomp.ser->object(clientInfo->activeEntity);
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
        case prot::cmd::CLIENT_INFO:
        {
            if (sendType == net::SendType::TCP && (flags & CMD_FLAG_RESP) == 0)
            {
                LG_I("Sending client info");
                prot::MsgComposer mcomp(net::SendType::TCP, tcpConnection);
                mcomp.startCommand(prot::cmd::CLIENT_INFO, CMD_FLAG_RESP);
                mcomp.ser->object(*clientInfo);
                mcomp.execute(sendQueue);
            }
            break;
        }
        case prot::cmd::NOTIFY_CLIENT_READY:
        {
            if (sendType == net::SendType::TCP && (flags & CMD_FLAG_RESP) == 0)
            {
                LG_I("Server accepted client readyness of {}",
                     clientInfo->name);
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
                cmddes.text1b(data,
                              dataEndPos - cmddes.adapter().currentReadPos());
                if (clientInfo)
                {
                    if (clientInfo->flags & CLIENT_FLAG_EN_CONSOLE)
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
        case prot::cmd::ACK_WORKSEQUENCER:
        {
            if (sendType == net::SendType::TCP && (flags & CMD_FLAG_RESP))
            {
                // todo: thread safety??
                clientInfo->ackWorkSequencer();
            }
            break;
        }
        case prot::cmd::SEL_CMD_MOVETO:
        {
            if (sendType == net::SendType::TCP && (flags & CMD_FLAG_RESP) == 0)
            {
                ecs::EntityId entityId;
                def::SectorCoords sectorCoords;
                prot::cmd::MoveToFlags moveToFlags;
                cmddes.object(entityId);
                cmddes.object(sectorCoords);
                cmddes.value1b(*((uint8_t*)&moveToFlags));
                entt::entity ent = ecs.getEntity(entityId);
                if (ent == entt::null)
                {
                    // todo: error handling?
                    return;
                }
                // todo: check if allowed
                auto* ai = ptrHandle->registry->try_get<ecs::Ai>(ent);
                if (ai)
                {
                    ai::TaskSystem* entityTaskSystem = &taskSystem;
                    if (auto* sectorId =
                            ptrHandle->registry->try_get<ecs::SectorId>(ent))
                    {
                        if (sectorId->id != world::INVALID_SECTOR_ID)
                        {
                            if (auto* sector = world.getSector(sectorId->id))
                            {
                                entityTaskSystem = &sector->getTaskSystem();
                            }
                        }
                    }
                    ai::TaskStackHandle stackHandle(ai->stackHandle);
                    if (moveToFlags.queue)
                    {
                        entityTaskSystem->addTaskLast(
                            stackHandle,
                            ai::taskdata::Goto{
                                .config = {.target = sectorCoords,
                                           .allowedPosError = 10.0f,
                                           .allowedRotError = 2.0f}});
                    }
                    else
                    {
                        entityTaskSystem->addTaskReplaceAll(
                            stackHandle,
                            ai::taskdata::Goto{
                                .config = {.target = sectorCoords,
                                           .allowedPosError = 10.0f,
                                           .allowedRotError = 2.0f}});
                    }
                    ai->nextRunFrame = ptrHandle->frameCnt + 1;
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
                LG_D("Sending all entt components to client {}",
                     clientInfo->name);
                sendAllEnttComponents(clientInfo, tcpConnection);
            }
            break;
        }
        case prot::cmd::THIRD_PERSON_CTRL:
        {
            if (sendType == net::SendType::TCP && (flags & CMD_FLAG_RESP) == 0)
            {
                def::ThirdPersonControl& thirdPersonControl =
                    clientInfo->thirdPersonControl;
                cmddes.object(thirdPersonControl);
                handleThirdPersonControl(clientInfo, tcpConnection);
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
    {
        ci->clearWorkSequencer();
        ci->clearActiveSectors();
        ci->clientInfo.connection = nullptr;
    }
    LG_I("TCP client disconnected (handle value={})", hv);
}

void Engine::sendAllEnttComponents(def::ClientInfo* clientInfo,
                                   net::TcpConnection* conn)
{
    clientInfo->addWorkFunction(
        [this, clientInfo, conn]()
        {
            prot::MsgComposer mcomp(net::SendType::TCP, conn);
            mcomp.startCommand(prot::cmd::ALL_ENTT_COMPONENTS, CMD_FLAG_RESP);
            mcomp.execute(sendQueue);
            return false;
        });
    uint32_t count = 0;
    ecs.iterateEntities(
        [this, clientInfo, conn, &count](ecs::EntityId entityId)
        {
            auto& reg = ecs.getRegistry();
            entt::entity ent = ecs.getEntity(entityId);
            auto* sectorId = reg.try_get<ecs::SectorId>(ent);
            // if (!reg.valid(ent) || !reg.all_of<ecs::tag::OOSSync>(ent))
            // {
            //     return;
            // }
            const ecs::EntityId entityCopy = entityId;
            clientInfo->addWorkFunction(
                [this, entityCopy, conn]()
                {
                    sendAllComponents(entityCopy, conn);
                    return false;
                });
            ++count;
            if (count % 1000 == 0)
            {
                clientInfo->addWorkFunction(
                    [this, clientInfo, conn]()
                    {
                        prot::MsgComposer mcomp(net::SendType::TCP, conn);
                        mcomp.startCommand(prot::cmd::ACK_WORKSEQUENCER, 0);
                        mcomp.execute(sendQueue);
                        return true;
                    });
            }
        });
    clientInfo->addWorkFunction(
        [this, clientInfo, conn]()
        {
            prot::MsgComposer mcomp(net::SendType::TCP, conn);
            mcomp.startCommand(prot::cmd::TOTAL_NUM_ENTITIES, 0);
            mcomp.ser->value4b(ecs.getNumEntities());
            mcomp.execute(sendQueue);
            return false;
        });
}

void Engine::broadcastEntityToClients(ecs::EntityId entityId)
{
    auto& reg = ecs.getRegistry();
    entt::entity ent = ecs.getEntity(entityId);
    for (def::ClientInfoHandle handle : connectedClientHandles)
    {
        def::ClientInfo* clientInfo = clientLib.getItem(handle);
        if (!clientInfo || !clientInfo->clientInfo.connection)
        {
            continue;
        }
        auto* sectorId = reg.try_get<ecs::SectorId>(ent);
        bool inActiveSector =
            sectorId && clientInfo->getActiveSectors().count(sectorId->id) > 0;
        if (!inActiveSector || !reg.valid(ent) || !reg.all_of<ecs::tag::OOSSync>(ent))
        {
            return;
        }
        sendAllComponents(entityId, clientInfo->clientInfo.connection);
    }
}

void Engine::sendAllComponents(ecs::EntityId entityId, net::TcpConnection* conn)
{
    entt::entity ent = ecs.getEntity(entityId);
    if (!ecs.getRegistry().valid(ent))
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
    auto& reg = ecs.getRegistry();
    for (int i = 0; i < 10000; ++i)
    {
        vec2 pos = vec2{posDist(gen), posDist(gen)};
        float rot = rotDist(gen);
        uint32_t sectorId = sectorPick(gen);
        ecs::EntityId ent;
        if (i % 4 == 0)
        {
            ent = entitySpawner->spawnShipHull(
                modManager.getHullLib().getHandle("Bee"),
                sectorId,
                ecs::Transform{pos, rot});
            entitySpawner->spawnModule(
                ent, modManager.getModuleLib().getHandle("Breeze"), 0);
            entitySpawner->spawnModule(
                ent, modManager.getModuleLib().getHandle("Breeze Maneuver"), 1);
            entitySpawner->spawnModule(
                ent, modManager.getModuleLib().getHandle("Breeze Maneuver"), 2);
            entitySpawner->spawnModule(
                ent,
                modManager.getModuleLib().getHandle("Small Mining Turret"),
                3);
            entitySpawner->spawnModule(
                ent,
                modManager.getModuleLib().getHandle("Small Mining Turret"),
                4);
            entitySpawner->spawnModule(
                ent, modManager.getModuleLib().getHandle("Terran Bulk S"), 5);
        }
        else if (i % 4 == 1)
        {
            ent = entitySpawner->spawnShipHull(
                modManager.getHullLib().getHandle("Mosquito"),
                sectorId,
                ecs::Transform{pos, rot});
            entitySpawner->spawnModule(
                ent, modManager.getModuleLib().getHandle("Breeze"), 0);
            entitySpawner->spawnModule(
                ent, modManager.getModuleLib().getHandle("Breeze Maneuver"), 1);
            entitySpawner->spawnModule(
                ent, modManager.getModuleLib().getHandle("Breeze Maneuver"), 2);
            entitySpawner->spawnModule(
                ent,
                modManager.getModuleLib().getHandle("Small Mining Turret"),
                3);
            entitySpawner->spawnModule(
                ent,
                modManager.getModuleLib().getHandle("Small Mining Turret"),
                4);
            entitySpawner->spawnModule(
                ent,
                modManager.getModuleLib().getHandle("Small Mining Turret"),
                5);
        }
        else if (i % 4 == 2)
        {
            ent = entitySpawner->spawnShipHull(
                modManager.getHullLib().getHandle("Bumblebee"),
                sectorId,
                ecs::Transform{pos, rot});
            entitySpawner->spawnModule(
                ent,
                modManager.getModuleLib().getHandle("Cargo Container S"),
                0);
            entitySpawner->spawnModule(
                ent,
                modManager.getModuleLib().getHandle("Cargo Container S"),
                1);
            entitySpawner->spawnModule(
                ent,
                modManager.getModuleLib().getHandle("Cargo Container S"),
                2);
            entitySpawner->spawnModule(
                ent,
                modManager.getModuleLib().getHandle("Cargo Container S"),
                3);
            entitySpawner->spawnModule(
                ent, modManager.getModuleLib().getHandle("Breeze"), 4);
            entitySpawner->spawnModule(
                ent, modManager.getModuleLib().getHandle("Breeze Maneuver"), 5);
            entitySpawner->spawnModule(
                ent, modManager.getModuleLib().getHandle("Breeze Maneuver"), 6);
        }
        else
        {
            ent = entitySpawner->spawnShipHull(
                modManager.getHullLib().getHandle("Caterpillar"),
                sectorId,
                ecs::Transform{pos, rot});

            for (int i = 0; i < 8; i++)
            {
                entitySpawner->spawnModule(
                    ent,
                    modManager.getModuleLib().getHandle("Small Mining Turret"),
                    i);
            }
            for (int i = 8; i < 16; i++)
            {
                entitySpawner->spawnModule(
                    ent,
                    modManager.getModuleLib().getHandle("Terran Bulk S"),
                    i);
            }
            entitySpawner->spawnModule(
                ent, modManager.getModuleLib().getHandle("Breeze"), 16);
            entitySpawner->spawnModule(
                ent,
                modManager.getModuleLib().getHandle("Breeze Maneuver"),
                17);
            entitySpawner->spawnModule(
                ent,
                modManager.getModuleLib().getHandle("Breeze Maneuver"),
                18);
        }
        auto* phyThrust = reg.try_get<ecs::PhyThrust>(ecs.getEntity(ent));
        if (phyThrust)
        {
            phyThrust->updateStatsFromEntity(ecs.getEntity(ent), ptrHandle);
        }
        auto* storage = reg.try_get<ecs::Storage>(ecs.getEntity(ent));
        if (storage)
        {
            storage->updateStatsFromEntity(ecs.getEntity(ent), ptrHandle);
        }
        auto* ai = reg.try_get<ecs::Ai>(ecs.getEntity(ent));
        if (ai)
        {
            ai::TaskSystem* entityTaskSystem = &taskSystem;
            if (auto* sectorId = reg.try_get<ecs::SectorId>(ecs.getEntity(ent)))
            {
                if (sectorId->id != world::INVALID_SECTOR_ID)
                {
                    if (auto* sector = world.getSector(sectorId->id))
                    {
                        entityTaskSystem = &sector->getTaskSystem();
                    }
                }
            }
            // auto* taskStack =
            // entityTaskSystem->getTaskStack(ai->stackHandle); if (taskStack)
            // {
            //     taskStack->setDefaultTask(ai::taskdata::UniversePatrol{
            //         .config = {.allowedPosError = 100.0f,
            //                    .allowedRotError = M_PIf}});
            // }
        }
    }

    static constexpr const char* kStationParts[] = {
        "ter-strut-4", "ter-strut-3", "ter-habitat-1"};
    static constexpr const char* kStationParts2[] = {"ter-solar-s",
                                                     "ter-cont-s"};
    static constexpr size_t kStationPartsCount =
        sizeof(kStationParts) / sizeof(kStationParts[0]);
    static constexpr size_t kStationParts2Count =
        sizeof(kStationParts2) / sizeof(kStationParts2[0]);

    for (int i = 0; i < 0; i++)
    {
        vec2 pos = vec2{posDist(gen), posDist(gen)};
        float rot = rotDist(gen);
        uint32_t sectorId = sectorPick(gen);
        ecs::EntityId stationId =
            entitySpawner->spawnStation(sectorId, ecs::Transform{pos, rot});

        const char* partName1 = kStationParts[rand() % kStationPartsCount];
        gobj::StationPartHandle partHandle1 =
            modManager.getStationPartLib().getHandle(partName1);

        gobj::StationPart* part1 =
            modManager.getStationPartLib().getItem(partHandle1);
        if (!part1)
        {
            LG_E("Spawn test station: station part '{}' not in library; skip",
                 partName1);
            continue;
        }
        ecs::EntityId partId =
            entitySpawner->addFirstStationPart(stationId, partHandle1, rot);
        if (partId == ecs::EntityId::Invalid())
        {
            continue;
        }

        for (int j = 0; j < part1->connectors.size(); j++)
        {
            const char* partName2 =
                kStationParts2[rand() % kStationParts2Count];
            gobj::StationPartHandle partHandle2 =
                modManager.getStationPartLib().getHandle(partName2);
            gobj::StationPart* part2 =
                modManager.getStationPartLib().getItem(partHandle2);
            if (!part2 || part2->connectors.empty())
            {
                continue;
            }
            entitySpawner->addStationPart(stationId,
                                          partId,
                                          partHandle2,
                                          j,
                                          rand() % part2->connectors.size());
        }

        // ecs::EntityId partId2 =
        //     addStationPart(stationId,
        //                    partId,
        //                    modManager.getStationPartLib().getHandle("strut-1"),
        //                    0,
        //                    0);
        // ecs::EntityId partId3 =
        //     addStationPart(stationId,
        //                    partId,
        //                    modManager.getStationPartLib().getHandle("strut-1"),
        //                    1,
        //                    1);
        // ecs::EntityId partId4 =
        //     addStationPart(stationId,
        //                    partId3,
        //                    modManager.getStationPartLib().getHandle("tank-1"),
        //                    0,
        //                    0);
        // ecs::EntityId partId5 =
        //     addStationPart(stationId,
        //                    partId4,
        //                    modManager.getStationPartLib().getHandle("strut-1"),
        //                    1,
        //                    0);
    }

    for (int i = 0; i < 100000; ++i)
    {
        vec2 pos1 = vec2{posDist(gen), posDist(gen)};
        vec2 pos2 = vec2{posDist(gen), posDist(gen)};
        uint32_t sectorId = sectorPick(gen);
        float rot1 = (rotDist(gen) - M_PIf) / 10.0f;
        float rot2 = (rotDist(gen) - M_PIf) / 10.0f;
        spawnAsteroid(sectorId,
                      ecs::Transform{pos1, rot1},
                      modManager.getAsteroidLib().getHandle("Small Asteroid 1"),
                      rot1);
        spawnAsteroid(sectorId,
                      ecs::Transform{pos2, rot2},
                      modManager.getAsteroidLib().getHandle("Small Asteroid 2"),
                      rot2);
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
            if (clientInfo->activeEntity != ecs::EntityId::Invalid())
            {
                entt::entity entity = ecs.getEntity(clientInfo->activeEntity);
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


void Engine::handleThirdPersonControl(def::ClientInfo* clientInfo,
                                      net::TcpConnection* conn)
{
    def::ThirdPersonControl& tpc = clientInfo->thirdPersonControl;
    ecs::EntityId entityId = clientInfo->activeEntity;
    entt::entity ent = ecs.getEntity(entityId);
    if (ent == entt::null)
    {
        LG_W("Entity not found for client {}", clientInfo->name);
        return;
    }
    auto* ai = ptrHandle->registry->try_get<ecs::Ai>(ent);
    bool shutdownAi = tpc.flags
                      & (def::ThirdPersonControl::FLG_DRIVE_MANUAL
                         | def::ThirdPersonControl::FLG_DRIVE_MANUAL
                         | def::ThirdPersonControl::FLG_FIRE_WEAPONS);
    if (ai && shutdownAi)
    {
        ai->active = false;
    }

    auto* hull = ptrHandle->registry->try_get<ecs::Hull>(ent);
    if (hull)
    {
        for (const auto& module : hull->modules)
        {
            if (module.slotType == gobj::ModuleType::Turret)
            {
                entt::entity turretEntt = ecs.getEntity(module.entityId);
                if (turretEntt != entt::null)
                {
                    auto* turret =
                        ptrHandle->registry->try_get<ecs::Turret>(turretEntt);
                    if (turret
                        && turret->aimMode == ecs::Turret::AimMode::Player)
                    {
                        turret->fireMode = ecs::Turret::FireMode::Manual;
                        turret->isFiring =
                            tpc.flags
                            & def::ThirdPersonControl::FLG_FIRE_WEAPONS;
                        auto& aimData =
                            std::get<ecs::Turret::PointData>(turret->aimData);
                        aimData.pos = tpc.ptrPos;
                    }
                }
            }
        }
    }
}

void Engine::spawnProjectile(uint32_t sectorId,
                             vec2 pos,
                             vec2 vel,
                             gobj::ProjectileHandle projectileHandle,
                             const ecs::EntityId& exceptEntity,
                             vec2 parVel)
{
    ecs::EntityId ent = entitySpawner->spawnProjectile(
        sectorId, pos, vel, projectileHandle, exceptEntity, parVel);
    if (ecs.validId(ent))
    {
        broadcastEntityToClients(ent);
    }
}

void Engine::spawnAsteroid(uint32_t sectorId,
                           const ecs::Transform& transform,
                           const gobj::AsteroidHandle& asteroidHandle,
                           float rotVel)
{
    ecs::EntityId ent = entitySpawner->spawnAsteroid(
        sectorId, transform, asteroidHandle, rotVel);
    if (ecs.validId(ent))
    {
        broadcastEntityToClients(ent);
    }
}

void Engine::spawnItem(uint32_t sectorId,
                       const ecs::Transform& transform,
                       const gobj::ItemHandle& itemHandle,
                       float quantity,
                       vec2 initialVel,
                       ecs::EntityId collexcept)
{
    ecs::EntityId ent = entitySpawner->spawnItem(
        sectorId, transform, itemHandle, quantity, initialVel, collexcept);
    if (ecs.validId(ent))
    {
        broadcastEntityToClients(ent);
    }
}

void Engine::loadCollisionMatrix()
{
    config.iterateThroughChildren(
        {"engine", "collision-layer-mat"},
        [this](const cfg::ConfigNode& node)
        {
            const string& pair = node.getName();
            size_t delim = pair.find('-');
            std::string first, second;
            if (delim != std::string::npos)
            {
                first = pair.substr(0, delim);
                second = pair.substr(delim + 1);
                std::vector<string> path = {"enabled"};
                bool enabled = static_cast<bool>(
                    std::get<float>(node.get(path, cfg::nodeVal_t(1.0f))));
                collisionLayerMat.setInteraction(
                    magic_enum::enum_cast<ecs::CollisionLayer>(first).value(),
                    magic_enum::enum_cast<ecs::CollisionLayer>(second).value(),
                    ecs::CollisionLayerMat::Interaction{.enabled = enabled});
            }
        });
}

void Engine::destroyEntity(ecs::EntityId entityId)
{
    auto entt = ecs.getEntity(entityId);
    for (auto compHelper : assetFactory.componentFactory.getComponentHelpers())
    {
        auto destroyFunc = compHelper.second.destroy;
        if (destroyFunc)
        {
            destroyFunc(ptrHandle, entt);
        }
    }
    if (ecs.destroyEntity(entityId))
    {
        forActiveClients(
            [this, entityId](def::ClientInfo* clientInfo)
            {
                prot::MsgComposer mcomp(net::SendType::TCP,
                                        clientInfo->clientInfo.connection);
                mcomp.startCommand(prot::cmd::DESTROY_ENTITY, 0);
                mcomp.ser->object(entityId);
                mcomp.execute(sendQueue);
            });
    }
}

void Engine::forActiveClients(
    std::function<void(def::ClientInfo* clientInfo)> callback)
{
    for (auto& clientHandle : activeClientHandles)
    {
        def::ClientInfo* clientInfo = clientLib.getItem(clientHandle);
        callback(clientInfo);
    }
}

}  // namespace sphys

template class con::ItemLib<net::ClientInfo>;
