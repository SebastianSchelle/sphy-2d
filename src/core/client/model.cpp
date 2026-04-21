#include "logging.hpp"
#include "std-inc.hpp"
#include <comp-gfx.hpp>
#include <comp-ident.hpp>
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
             mod::ModManager* modManager,
             gfx::RenderEngine* renderer,
             std::function<void(void)> afterLoadWorldClb)
    : userInterface(userInterface), config(config), modManager(modManager),
      renderer(renderer), afterLoadWorldClb(afterLoadWorldClb)
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
    cFac->registerComponent<ecs::Collider>();
    cFac->registerComponent<ecs::Broadphase>();
    cFac->registerComponent<ecs::TransformCache>();
    cFac->registerComponent<ecs::MapIcon>();
    cFac->registerComponent<ecs::Textures>();

    lastGetAabbTree = tim::nowU();
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
                notifyReady();
            }
            break;
        case ClientGameState::NotifyServerReady:
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
            return prot::writeCommand(
                cmdser,
                prot::cmd::TIME_SYNC,
                0,
                [this](bitsery::Serializer<OutputAdapter>& cmdser)
                { return true; });
        },
        true);
}

void Model::modelLoopMenu(float dt) {}

void Model::modelLoopGame(float dt)
{
    tim::Timepoint now = tim::getCurrentTimeU();
    static tim::Timepoint testTime = tim::getCurrentTimeU();
    static tim::Timepoint lastReqAllComponents = tim::getCurrentTimeU();
    static tim::Timepoint lastGetAabbTree = tim::getCurrentTimeU();

    if (renderer->getViewMode() == gfx::GameViewMode::ThirdPerson)
    {
        entt::entity activeEntity = ecs.getEntity(clientInfo.getActiveEntity());
        auto& reg = ecs.getRegistry();
        if (reg.valid(activeEntity))
        {
            auto* transform = reg.try_get<ecs::Transform>(activeEntity);
            auto* sectorId = reg.try_get<ecs::SectorId>(activeEntity);
            if (transform && sectorId)
            {
                renderer->panWorldTo(def::SectorCoords{
                    .pos = world.idToSectorCoords(sectorId->id),
                    .sectorPos = transform->pos,
                });
            }
        }
    }
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
                       [this]()
                       { reqAllComponents(clientInfo.getActiveEntity()); });
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

            parseCommand(
                cmddes, cmdData.sendType, cmd, flags, dataStartPos + len);
            size_t readPos = cmddes.adapter().currentReadPos();
            if (readPos - dataStartPos != len)
            {
                LG_W("Command data length mismatch. Expected: {}, Read: {}",
                     len,
                     cmddes.adapter().currentReadPos() - dataStartPos);
                return;
            }
        }
    }
    catch (const std::exception& e)
    {
        LG_E("Error parsing command message: {}", e.what());
    }
}

void Model::parseCommand(bitsery::Deserializer<InputAdapter>& cmddes,
                         net::SendType sendType,
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
            if (flags & CMD_FLAG_RESP)
            {
                uint64_t t1;
                cmddes.value8b(t1);
                if (timeSyncData.waiting)
                {
                    timeSyncData.waiting = false;
                    // Server time at request arrival
                    // Now
                    timeSyncData.t1 = t1;
                    timeSyncData.t2 = tim::nowU();
                    // Travel time from client to server and back again
                    long rtt = timeSyncData.t2 - timeSyncData.t0;

                    // latency = half travel time
                    timeSyncData.latency[timeSyncData.cnt] = rtt / 2;
                    // Server time = server time at request arrival +
                    // latency
                    long serverTime = timeSyncData.t1
                                      + timeSyncData.latency[timeSyncData.cnt];
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
            if (flags & CMD_FLAG_RESP && sendType == net::SendType::TCP)
            {
                LG_I("Authentication successful");
                gameState = ClientGameState::Authenticated;
            }
            break;
        }
        case prot::cmd::WORLD_INFO:
        {
            if (flags & CMD_FLAG_RESP && sendType == net::SendType::TCP)
            {
                def::WorldShape worldShape;
                cmddes.object(worldShape);
                world.createFromServer(worldShape);
            }
            break;
        }
        case prot::cmd::NOTIFY_CLIENT_READY:
        {
            if (flags & CMD_FLAG_RESP && sendType == net::SendType::TCP)
            {
                LG_I("Server accepted client readyness");
                gameState = ClientGameState::GameLoop;
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
            break;
        }
        case prot::cmd::DBG_GET_AABB_TREE:
        {
            if (flags & CMD_FLAG_RESP)
            {
                handleGetAabbTreeResp(cmddes, posNextCmdOrEof);
            }
            break;
        }
        case prot::cmd::ACTIVE_ENTITY_SWITCHED:
        {
            if ((flags & CMD_FLAG_RESP) == 0)
            {
                handleActiveEntitySwitched(cmddes, posNextCmdOrEof);
            }
            break;
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

void Model::drawTacticalMap(gfx::RenderEngine& renderer,
                            const glm::vec4& viewRect,
                            float zoom)
{
    world.drawTacticalMap(renderer, viewRect, zoom);
    drawTextures(renderer, viewRect, zoom);
    auto& reg = ecs.getRegistry();
    for (const auto& entityId : selectedEntities)
    {
        entt::entity entity = ecs.getEntity(entityId);
        if (reg.valid(entity))
        {
            auto* trans = reg.try_get<ecs::Transform>(entity);
            auto* sectorId = reg.try_get<ecs::SectorId>(entity);
            if (trans && sectorId)
            {
                glm::vec2 worldPos =
                    world.getWorldPosSectorOffset(sectorId->id,
                                                  renderer.getSectorOffsetX(),
                                                  renderer.getSectorOffsetY())
                    + trans->pos;
                renderer.drawRectangle(worldPos,
                                       glm::vec2(40.0f, 40.0f),
                                       0xff004000,
                                       1.0f / zoom,
                                       0.0f,
                                       0);
            }
        }
    }

    // reg.view<ecs::Transform, ecs::SectorId, ecs::Colllider,
    // ecs::Broadphase>()
    //     .each(
    //         [this, &renderer, &viewRect, zoom](ecs::Transform& transform,
    //                                            ecs::SectorId& sectorId,
    //                                            ecs::Colllider& collider,
    //                                            ecs::Broadphase&
    //                                            broadphase)
    //         {
    //             glm::vec2 worldPos =
    //                 world.getWorldPosSectorOffset(sectorId.id,
    //                                               renderer.getSectorOffsetX(),
    //                                               renderer.getSectorOffsetY())
    //                 + transform.pos;
    //             if (smath::pointInsideRect(worldPos, viewRect))
    //             {
    //                 uint8_t size = 1.0;
    //                 for (const auto& vertex : collider.vertices)
    //                 {
    //                     float c = cos(transform.rot);
    //                     float s = sin(transform.rot);
    //                     glm::vec2 rotatedVertex =
    //                         glm::vec2(c * vertex.x - s * vertex.y,
    //                                   s * vertex.x + c * vertex.y);
    //                     glm::vec2 vertexWorldPos = worldPos +
    //                     rotatedVertex; renderer.drawEllipse(
    //                         vertexWorldPos,
    //                         glm::vec2(size / zoom, size / zoom),
    //                         0xffffffff,
    //                         2.0f / zoom,
    //                         0.0f,
    //                         0);
    //                     size += 1.0;
    //                 }
    //                 auto fatAABB = broadphase.fatAABB;
    //                 renderer.drawRectangle(
    //                     worldPos,
    //                     vec2(fatAABB.upper.x - fatAABB.lower.x,
    //                          fatAABB.upper.y - fatAABB.lower.y),
    //                     0xffffffff,
    //                     1.0f / zoom,
    //                     0.0f,
    //                     0);
    //             }
    //         });
    if (overlayAabbTreeEnabled)
    {
        drawOverlayAABBs(renderer, zoom);
    }
}

void Model::drawStrategicMap(gfx::RenderEngine& renderer,
                             const glm::vec4& viewRect,
                             float zoom)
{
    world.drawStrategicMap(renderer, viewRect, zoom);
    // todo: Group entities by Pos and only show lists or fleets or groups
    auto& reg = ecs.getRegistry();
    reg.view<ecs::Transform, ecs::SectorId, ecs::MapIcon>().each(
        [this, &renderer, &viewRect](ecs::Transform& transform,
                                     ecs::SectorId& sectorId,
                                     ecs::MapIcon& mapIcon)
        {
            glm::vec2 worldPos =
                world.getWorldPosSectorOffset(sectorId.id,
                                              renderer.getSectorOffsetX(),
                                              renderer.getSectorOffsetY())
                + transform.pos;
            if (smath::pointInsideRect(worldPos, viewRect))
            {
                mod::MappedTextureHandle mTexHandle =
                    *(mod::MappedTextureHandle*)&mapIcon.texHandle;
                const mod::MappedTexture* mappedTexture =
                    modManager->getResourceMap().getMappedTexture(mTexHandle);
                gfx::TextureHandle texHandle = gfx::TextureHandle::Invalid();
                if (mappedTexture)
                {
                    texHandle = mappedTexture->texHandle;
                }
                renderer.drawTexRect(worldPos,
                                     glm::vec2(mapIcon.size.x, mapIcon.size.y),
                                     texHandle,
                                     transform.rot,
                                     0);
            }
        });
    if (overlayAabbTreeEnabled)
    {
        drawOverlayAABBs(renderer, zoom);
    }
}

void Model::drawThirdPerson(gfx::RenderEngine& renderer,
                            const glm::vec4& viewRect,
                            float zoom)
{
    world.drawThirdPerson(renderer, viewRect, zoom);
    drawTextures(renderer, viewRect, zoom);
}

void Model::drawTextures(gfx::RenderEngine& renderer,
                         const glm::vec4& viewRect,
                         float zoom)
{
    auto& reg = ecs.getRegistry();
    reg.view<ecs::Transform, ecs::SectorId, ecs::Textures>().each(
        [this, &renderer, &viewRect](ecs::Transform& transform,
                                     ecs::SectorId& sectorId,
                                     ecs::Textures& textures)
        {
            glm::vec2 worldPos =
                world.getWorldPosSectorOffset(sectorId.id,
                                              renderer.getSectorOffsetX(),
                                              renderer.getSectorOffsetY())
                + transform.pos;
            if (smath::pointInsideRect(worldPos, viewRect))
            {
                for (const auto& texture : textures.textures)
                {
                    mod::MappedTextureHandle mTexHandle =
                        *(mod::MappedTextureHandle*)&texture.texHandle;
                    const mod::MappedTexture* mappedTexture =
                        modManager->getResourceMap().getMappedTexture(
                            mTexHandle);
                    gfx::TextureHandle texHandleGFX =
                        gfx::TextureHandle::Invalid();
                    if (mappedTexture)
                    {
                        texHandleGFX = mappedTexture->texHandle;
                    }
                    vec2 texOffset = smath::rotateVec2(
                        vec2(texture.bounds.x, texture.bounds.y),
                        -transform.rot);
                    renderer.drawTexRect(
                        worldPos + texOffset,
                        glm::vec2(texture.bounds.z, texture.bounds.w),
                        texHandleGFX,
                        transform.rot + texture.rot,
                        texture.zIndex / 100.0f,
                        0);
                }
            }
        });
}

void Model::setOverlayEnabled(const std::string& overlay, bool enabled)
{
    if (overlay == "aabb-tree")
    {
        overlayAabbTreeEnabled = enabled;
        if (!overlayAabbTreeEnabled)
        {
            aabbs.clear();
        }
    }
}

bool Model::isAabbTreeOverlayEnabled() const
{
    return overlayAabbTreeEnabled;
}

void Model::sendCmdToServer(const std::string& command)
{
    prot::MsgComposer mcomp(net::SendType::TCP, nullptr);
    mcomp.startCommand(prot::cmd::CONSOLE_CMD, 0);
    mcomp.ser->text1b(command, command.size());
    mcomp.execute(sendQueue);
}

void Model::checkVersion(const net::ModelClientInfo& clientInfo)
{
    this->clientInfo =
        def::ClientInfo("", clientInfo, def::ClientFlags{.enConsole = 0});
    prot::MsgComposer mcomp(net::SendType::TCP, nullptr);
    mcomp.startCommand(prot::cmd::VERSION_CHECK, 0);
    mcomp.ser->value2b(version::MAJOR);
    mcomp.ser->value2b(version::MINOR);
    mcomp.ser->value2b(version::PATCH);
    mcomp.execute(sendQueue);

    gameState = ClientGameState::VersionCheck;
}

void Model::authenticate()
{
    LG_I("Authenticating with server...");
    prot::MsgComposer mcomp(net::SendType::TCP, nullptr);
    mcomp.startCommand(prot::cmd::AUTHENTICATE, 0);
    mcomp.ser->value2b(version::MAJOR);
    mcomp.ser->value2b(version::MINOR);
    mcomp.ser->value2b(version::PATCH);
    mcomp.ser->text1b(clientInfo.modelClientInfo.token, 16);
    mcomp.ser->value2b((uint16_t)clientInfo.modelClientInfo.udpPortCli);
    mcomp.execute(sendQueue);
    gameState = ClientGameState::Authenticating;
}

void Model::notifyReady()
{
    prot::MsgComposer mcomp(net::SendType::TCP, nullptr);
    mcomp.startCommand(prot::cmd::NOTIFY_CLIENT_READY, 0);
    mcomp.execute(sendQueue);
    gameState = ClientGameState::NotifyServerReady;
    LG_I("Notifying server ready");
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
                    reg.emplace_or_replace<ecs::SectorId>(
                        entity, sectorId, x, y);
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
    cmddes.object(entityId);
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
        else
        {
            LG_W("Unknown component hash: {}", compHash);
            return;
        }
    }
}

void Model::reqAllComponents(ecs::EntityId entityId)
{
    prot::MsgComposer mcomp(net::SendType::TCP, nullptr);
    mcomp.startCommand(prot::cmd::REQ_ALL_COMPONENTS, 0);
    mcomp.ser->object(entityId);
    mcomp.execute(sendQueue);
}

void Model::selectEntitiesInsideRect(const def::SectorCoords& start,
                                     const def::SectorCoords& end)
{
    selectedEntities.clear();
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
    std::size_t idx = 0;
    if (selectedEntities.empty())
    {
        return;
    }
    prot::MsgComposer mcomp(net::SendType::TCP, nullptr);
    for (auto& entityId : selectedEntities)
    {
        mcomp.startCommand(prot::cmd::ENT_CMD_MOVETO_POS, 0);
        mcomp.ser->object(entityId);
        mcomp.ser->object(sectorCoords);
        if (mcomp.ser->adapter().currentWritePos()
            > prot::kMaxSerializedChunkBytes
                  - (sizeof(ecs::EntityId) + sizeof(def::SectorCoords)))
        {
            mcomp.execute(sendQueue);
            mcomp.resetData();
        }
    }
    mcomp.execute(sendQueue);
}

void Model::handleGetAabbTreeResp(bitsery::Deserializer<InputAdapter>& cmddes,
                                  uint16_t posNextCmdOrEof)
{
    aabbs.clear();
    cmddes.value4b(aabbSector);
    cmddes.object(aabbs);
}

void Model::drawOverlayAABBs(gfx::RenderEngine& renderer, float zoom)
{
    auto now = tim::nowU();
    if (!aabbs.empty())
    {
        for (const auto& aabb : aabbs)
        {
            glm::vec2 worldPos =
                world.getWorldPosSectorOffset(aabbSector,
                                              renderer.getSectorOffsetX(),
                                              renderer.getSectorOffsetY());
            vec2 pos = worldPos + (aabb.lower + aabb.upper) / 2.0f;
            vec2 size = aabb.upper - aabb.lower;
            renderer.drawRectangle(pos, size, 0x10ffffff, 1.0f / zoom, 0.0f, 0);
        }
    }

    auto sendGetAabbTree = [this, &renderer]()
    {
        prot::MsgComposer mcomp(net::SendType::TCP, nullptr);
        mcomp.startCommand(prot::cmd::DBG_GET_AABB_TREE, 0);
        uint32_t sectorId = world.sectorCoordsToId(renderer.getSectorOffsetX(),
                                                   renderer.getSectorOffsetY());
        mcomp.ser->value4b(sectorId);
        mcomp.execute(sendQueue);
    };
    DO_PERIODIC_U_EXTNOW(lastGetAabbTree, 100000, now, sendGetAabbTree);
}

void Model::handleActiveEntitySwitched(
    bitsery::Deserializer<InputAdapter>& cmddes,
    uint16_t posNextCmdOrEof)
{
    ecs::EntityId entityId;
    cmddes.object(entityId);
    clientInfo.setActiveEntity(entityId);
}

}  // namespace sphyc