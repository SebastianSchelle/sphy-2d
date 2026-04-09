#include "logging.hpp"
#include "std-inc.hpp"
#include <components/comp-ident.hpp>
#include <exchange-sequence.hpp>
#include <model.hpp>
#include <protocol.hpp>
#include <ui/user-interface.hpp>
#include <version.hpp>
#include <world-def.hpp>

namespace sphyc
{

Model::Model(ui::UserInterface* userInterface,
             cfg::ConfigManager& config,
             std::function<void(void)> afterLoadWorldClb)
    : userInterface(userInterface), config(config),
      afterLoadWorldClb(afterLoadWorldClb)
{
    loadWorldSequence.registerExchange(net::Exchange(
        prot::cmd::WORLD_INFO,
        [this]() {},
        [this]() {},
        [this](bitsery::Serializer<OutputAdapter>& ser) {}));
    lastTSync = tim::getCurrentTimeU();

    auto cFac = &assetFactory.componentFactory;
    cFac->registerComponent<ecs::Transform>();
    cFac->registerComponent<ecs::PhysicsBody>();
    cFac->registerComponent<ecs::AssetId>();
    cFac->registerComponent<ecs::PhyThrust>();
    cFac->registerComponent<ecs::MoveCtrl>();

    selectedEntity = {0, 1};
}

Model::~Model() {}

void Model::modelLoop(float dt)
{
    net::CmdQueueData recQueueData;
    while (receiveQueue.try_dequeue(recQueueData))
    {
        parseCommandData(recQueueData);
    }
    switch (gameState)
    {
        case ClientGameState::Init:
            break;
        case ClientGameState::MainMenu:
            modelLoopMenu(dt);
            break;
        case ClientGameState::VersionCheck:
            break;
        case ClientGameState::Authenticated:
            loadWorldSequence.start(sendQueue);
            gameState = ClientGameState::LoadWorld;
            break;
        case ClientGameState::LoadWorld:
            if (loadWorldSequence.done())
            {
                LG_I("Exchanging world info with server done");
                afterLoadWorldClb();
                gameState = ClientGameState::GameLoop;
            }
            break;
        case ClientGameState::GameLoop:
            modelLoopGame(dt);
            break;
        default:
            break;
    }
}

void Model::startLoadingMods()
{
    gameState = ClientGameState::LoadingMods;
}

void Model::startModel()
{
    gameState = ClientGameState::MainMenu;
}

void Model::timeSync()
{
    if (timeSyncData.waiting)
    {
        if ((tim::nowU() - timeSyncData.t0) > 1000000)
        {
            timeSyncData.waiting = false;
        }
        else
        {
            return;
        }
    }
    timeSyncData.t0 = tim::nowU();
    timeSyncData.waiting = true;
    prot::writeMessageUdp(
        sendQueue,
        nullptr,
        [this](bitsery::Serializer<OutputAdapter>& cmdser)
        {
            prot::writeCommand(
                cmdser,
                prot::cmd::TIME_SYNC,
                0,
                [this](bitsery::Serializer<OutputAdapter>& cmdser) {});
        },
        true);
}

void Model::modelLoopMenu(float dt) {}

void Model::modelLoopGame(float dt)
{
    tim::Timepoint now = tim::getCurrentTimeU();
    static tim::Timepoint testTime = tim::getCurrentTimeU();
    static tim::Timepoint lastReqAllComponents = tim::getCurrentTimeU();

    // Send some stuff to server
    /*CMDAT_PREP_TOKEN(net::SendType::UDP, prot::cmd::LOG, 0)
    std::string str = "Hello World!";
    cmdser.text1b(str, str.size());
    CMDAT_FIN_TOKEN()
    sendQueue.enqueue(cmdData);*/

    /*DO_PERIODIC_EXTNOW(testTime, 1000000, now, [this]() {
        prot::writeMessageUdp(sendQueue, nullptr,
    [this](bitsery::Serializer<OutputAdapter>& cmdser) {
            prot::writeCommand(cmdser, prot::cmd::LOG, 0,
    [this](bitsery::Serializer<OutputAdapter>& cmdser) { std::string str =
    "Hello World!"; cmdser.text1b(str, str.size());
            });
            prot::writeCommand(cmdser, prot::cmd::LOG, 0,
    [this](bitsery::Serializer<OutputAdapter>& cmdser) { std::string str =
    "Hello Sector!"; cmdser.text1b(str, str.size());
            });
        }, true);
    });*/

    if (timeSyncData.cnt == 0)
    {
        DO_PERIODIC_EXTNOW(lastTSync, 2000000, now, [this]() { timeSync(); });
    }
    else
    {
        DO_PERIODIC_EXTNOW(lastTSync, 50000, now, [this]() { timeSync(); });
    }

    DO_PERIODIC_EXTNOW(lastReqAllComponents,
                       1000000,
                       now,
                       [this]() { reqAllComponents(selectedEntity); });
}

void Model::parseCommandData(const net::CmdQueueData& cmdData)
{
    try
    {
        const std::vector<uint8_t>& data = cmdData.data;
        bitsery::Deserializer<InputAdapter> cmddes(
            InputAdapter{data.begin(), data.size()});

        if (cmdData.sendType == net::SendType::UDP)
        {
        }
        else if (cmdData.sendType == net::SendType::TCP)
        {
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
            parseCommand(cmddes, cmd, flags, dataStartPos + len);
            cmddes.adapter().currentReadPos(dataStartPos + len);
        }
    }
    catch (const std::exception& e)
    {
        LG_E("Error parsing command message: {}", e.what());
    }
}

void Model::parseCommand(bitsery::Deserializer<InputAdapter>& cmddes,
                         uint16_t cmd,
                         uint8_t flags,
                         uint16_t posNextCmdOrEof)
{
    prot::cmd::State result = prot::cmd::State::SUCCESS;

    switch (cmd)
    {
        case prot::cmd::LOG:
        {
            std::string str;
            cmddes.text1b(str, posNextCmdOrEof);
            LG_I("Log from server: {}", str);
            break;
        }
        case prot::cmd::TIME_SYNC:
        {
            if (timeSyncData.waiting && flags & CMD_FLAG_RESP
                && posNextCmdOrEof == 8)
            {
                timeSyncData.waiting = false;
                // Server time at request arrival
                cmddes.value8b(timeSyncData.t1);
                // Now
                timeSyncData.t2 = tim::nowU();
                // Travel time from client to server and back again
                long rtt = timeSyncData.t2 - timeSyncData.t0;

                // latency = half travel time
                timeSyncData.latency[timeSyncData.cnt] = rtt / 2;
                // Server time = server time at request arrival + latency
                long serverTime =
                    timeSyncData.t1 + timeSyncData.latency[timeSyncData.cnt];
                timeSyncData.offset[timeSyncData.cnt] =
                    serverTime - timeSyncData.t2;
                timeSyncData.cnt++;
                if (timeSyncData.cnt == 10)
                {
                    long latMin = 1000000000;
                    long offsMin;
                    for (uint i = 0; i < 10; ++i)
                    {
                        if (timeSyncData.latency[i] < latMin)
                        {
                            latMin = timeSyncData.latency[i];
                            offsMin = timeSyncData.offset[i];
                        }
                    }
                    timeSyncData.serverOffset = offsMin / 1.0e6f;
                    timeSyncData.serverLatency = latMin / 1.0e6f;
                    timeSyncData.cnt = 0;
                }
                timeSyncData.waiting = false;
            }
            break;
        }
        case prot::cmd::VERSION_CHECK:
        {
            if (flags & CMD_FLAG_RESP)
            {
                uint16_t major;
                uint16_t minor;
                uint16_t patch;
                cmddes.value2b(major);
                cmddes.value2b(minor);
                cmddes.value2b(patch);
                if (major != version::MAJOR)
                {
                    LG_E(
                        "Cannot connect to server. Version mismatch. "
                        "Server: "
                        "{}.{}.{}, Client: {}.{}.{}",
                        major,
                        minor,
                        patch,
                        version::MAJOR,
                        version::MINOR,
                        version::PATCH);
                    return;
                }
                else
                {
                    if (minor != version::MINOR || patch != version::PATCH)
                    {
                        LG_W(
                            "Version mismatch. Server: {}.{}.{}, Client: "
                            "{}.{}.{}",
                            major,
                            minor,
                            patch,
                            version::MAJOR,
                            version::MINOR,
                            version::PATCH);
                    }
                    LG_I("Version check successful");
                    authenticate();
                }
            }
            break;
        }
        case prot::cmd::AUTHENTICATE:
        {
            if (flags & CMD_FLAG_RESP)
            {
                LG_I("Authentication successful");
                gameState = ClientGameState::Authenticated;
            }
            break;
        }
        case prot::cmd::WORLD_INFO:
        {
            if (flags & CMD_FLAG_RESP)
            {
                def::WorldShape worldShape;
                cmddes.object(worldShape);
                world.createFromServer(worldShape);
            }
            break;
        }
        case prot::cmd::CONSOLE_CMD:
        {
            if (flags & CMD_FLAG_RESP)
            {
                std::string str;
                cmddes.text1b(str, posNextCmdOrEof);
                LG_I("Console cmd response: {}", str);
                userInterface->addSystemMessage(str);
            }
            break;
        }
        case prot::cmd::SLOW_DUMP:
        {
            handleSlowDump(cmddes, posNextCmdOrEof);
            break;
        }
        case prot::cmd::REQ_ALL_COMPONENTS:
        {
            if (flags & CMD_FLAG_RESP)
            {
                handleReqAllComponentsResp(cmddes, posNextCmdOrEof);
            }
        }
        default:
            break;
    }

    // Callbacks for custom commands...

    // Check exchange sequence progress
    if (gameState == ClientGameState::LoadWorld && !loadWorldSequence.done())
    {
        loadWorldSequence.advance(sendQueue, cmd, result);
    }
}

void Model::drawDebug(gfx::RenderEngine& renderer, float zoom)
{
    world.drawDebug(renderer, zoom);
    auto& reg = ecs.getRegistry();
    reg.view<ecs::Transform, ecs::SectorId>().each(
        [this, &renderer](ecs::Transform& transform, ecs::SectorId& sectorId)
        {
            glm::vec2 worldPos =
                world.getWorldPosSectorOffset(sectorId.id,
                                              renderer.getSectorOffsetX(),
                                              renderer.getSectorOffsetY())
                + transform.pos;
            renderer.drawEllipse(worldPos,
                                 glm::vec2(10.0f, 5.0f),
                                 0xffffffff,
                                 2.0f,
                                 transform.rot,
                                 0);
        });
}

void Model::sendCmdToServer(const std::string& command)
{
    prot::writeMessageTcp(
        sendQueue,
        nullptr,
        [this, command](bitsery::Serializer<OutputAdapter>& cmdser)
        {
            prot::writeCommand(
                cmdser,
                prot::cmd::CONSOLE_CMD,
                0,
                [this, command](bitsery::Serializer<OutputAdapter>& cmdser)
                { cmdser.text1b(command, command.size()); });
        });
}

void Model::checkVersion(const net::ModelClientInfo& clientInfo)
{
    this->clientInfo = clientInfo;
    prot::writeMessageTcp(
        sendQueue,
        nullptr,
        [this](bitsery::Serializer<OutputAdapter>& cmdser)
        {
            prot::writeCommand(
                cmdser,
                prot::cmd::VERSION_CHECK,
                0,
                [this](bitsery::Serializer<OutputAdapter>& cmdser)
                {
                    cmdser.value2b(version::MAJOR);
                    cmdser.value2b(version::MINOR);
                    cmdser.value2b(version::PATCH);
                });
        });

    gameState = ClientGameState::VersionCheck;
}

void Model::authenticate()
{
    LG_I("Authenticating with server...");
    prot::writeMessageTcp(
        sendQueue,
        nullptr,
        [this](bitsery::Serializer<OutputAdapter>& cmdser)
        {
            prot::writeCommand(
                cmdser,
                prot::cmd::AUTHENTICATE,
                0,
                [this](bitsery::Serializer<OutputAdapter>& cmdser)
                {
                    cmdser.value2b(version::MAJOR);
                    cmdser.value2b(version::MINOR);
                    cmdser.value2b(version::PATCH);
                    cmdser.text1b(clientInfo.token, 16);
                    cmdser.value2b((uint16_t)clientInfo.udpPortCli);
                });
        });
    gameState = ClientGameState::Authenticating;
}

void Model::disconnectFromServer()
{
    switch (gameState)
    {
        case ClientGameState::Authenticating:
            LG_W("Authentication refused");
            gameState = ClientGameState::MainMenu;
            break;
        case ClientGameState::GameLoop:
            LG_W("Disconnecting from server");
            gameState = ClientGameState::MainMenu;
            break;
        default:
            break;
    }
}

void Model::handleSlowDump(bitsery::Deserializer<InputAdapter>& cmddes,
                           uint16_t posNextCmdOrEof)
{
    uint32_t compHash;
    cmddes.value4b(compHash);
    for (auto& [hash, helper] :
         assetFactory.componentFactory.getComponentHelpers())
    {
        if (hash == compHash)
        {
            while (cmddes.adapter().currentReadPos() < posNextCmdOrEof - 6)
            {
                uint32_t sectorId;
                uint16_t numEntities;
                cmddes.value4b(sectorId);
                cmddes.value2b(numEntities);
                for (uint i = 0; i < numEntities; ++i)
                {
                    ecs::EntityId entityId;
                    cmddes.object(entityId);
                    entt::entity entity = ecs.enttFromServerId(entityId);

                    auto& reg = ecs.getRegistry();
                    auto [x, y] = world.idToSectorCoords(sectorId);
                    reg.emplace_or_replace<ecs::SectorId>(entity, sectorId, x, y);
                    reg.emplace_or_replace<ecs::EntityId>(entity, entityId);
                    helper.deserializeIntoRegistry(reg, entity, cmddes);
                }
            }
        }
    }
}

void Model::handleReqAllComponentsResp(
    bitsery::Deserializer<InputAdapter>& cmddes,
    uint16_t posNextCmdOrEof)
{
    ecs::EntityId entityId;
    cmddes.value4b(entityId.index);
    cmddes.value2b(entityId.generation);
    entt::entity entity = ecs.enttFromServerId(entityId);
    while (cmddes.adapter().currentReadPos() < posNextCmdOrEof - 4)
    {
        uint32_t compHash;
        cmddes.value4b(compHash);
        auto compHelper = assetFactory.componentFactory.getComponentHelpers();
        auto it = compHelper.find(compHash);
        if (it != compHelper.end())
        {
            auto& reg = ecs.getRegistry();
            it->second.deserializeIntoRegistry(reg, entity, cmddes);
        }
    }
}

void Model::reqAllComponents(ecs::EntityId entityId)
{
    prot::writeMessageTcp(
        sendQueue,
        nullptr,
        [this, entityId](bitsery::Serializer<OutputAdapter>& cmdser)
        {
            prot::writeCommand(
                cmdser,
                prot::cmd::REQ_ALL_COMPONENTS,
                0,
                [this, entityId](bitsery::Serializer<OutputAdapter>& cmdser)
                {
                    cmdser.value4b(entityId.index);
                    cmdser.value2b(entityId.generation);
                });
        });
}

void Model::selectEntitiesInsideRect(const def::SectorCoords& start,
                                     const def::SectorCoords& end)
{
    auto& reg = ecs.getRegistry();
    auto& xMin = def::SectorCoords::minX(start, end);
    auto& xMax = def::SectorCoords::maxX(start, end);
    auto& yMin = def::SectorCoords::minY(start, end);
    auto& yMax = def::SectorCoords::maxY(start, end);
    reg.view<ecs::SectorId, ecs::Transform, ecs::EntityId>().each(
        [this, &xMin, &xMax, &yMin, &yMax](
            ecs::SectorId& sid, ecs::Transform& tr, ecs::EntityId& eid)
        {
            bool xMinBool =
                sid.x > xMin.pos.x
                || (sid.x == xMin.pos.x && tr.pos.x > xMin.sectorPos.x);
            bool xMaxBool =
                sid.x < xMax.pos.x
                || (sid.x == xMax.pos.x && tr.pos.x < xMax.sectorPos.x);
            bool yMinBool =
                sid.y > yMin.pos.y
                || (sid.y == yMin.pos.y && tr.pos.y > yMin.sectorPos.y);
            bool yMaxBool =
                sid.y < yMax.pos.y
                || (sid.y == yMax.pos.y && tr.pos.y < yMax.sectorPos.y);
            if (xMinBool && xMaxBool && yMinBool && yMaxBool)
            {
                selectedEntities.push_back(eid);
            }
        });
}

void Model::clearSelectedEntities()
{
    selectedEntities.clear();
}

void Model::selectedEntitiesMoveCmd(def::SectorCoords& sectorCoords)
{
    prot::writeMessageTcp(
        sendQueue,
        nullptr,
        [this, sectorCoords](bitsery::Serializer<OutputAdapter>& cmdser)
        {
            for (auto& entityId : selectedEntities)
            {
                prot::writeCommand(
                    cmdser,
                    prot::cmd::ENT_CMD_MOVETO_POS,
                    0,
                    [this, entityId, sectorCoords](
                        bitsery::Serializer<OutputAdapter>& cmdser)
                    {
                        cmdser.object(entityId);
                        cmdser.object(sectorCoords);
                    });
            }
        });
}

}  // namespace sphyc