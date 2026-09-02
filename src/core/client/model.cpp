#include "aabb-tree.hpp"
#include "client-pool-obj.hpp"
#include "comp-phy.hpp"
#include "config-manager.hpp"
#include "lib-modules.hpp"
#include "lib-projectile.hpp"
#include "lib-textures.hpp"
#include "logging.hpp"
#include "render-engine.hpp"
#include "sector.hpp"
#include "std-inc.hpp"
#include <comp-gfx.hpp>
#include <comp-ident.hpp>
#include <comp-struct.hpp>
#include <comp-tag.hpp>
#include <comp-turret.hpp>
#include <exchange-sequence.hpp>
#include <model.hpp>
#include <protocol.hpp>
#include <user-interface.hpp>
#include <version.hpp>
#include <world-def.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>


#define OPOOL_RECV(name, opool_n, junkSize, block)                             \
    case prot::cmd::SEND_BEGIN_##name:                                         \
        opool_n.markInactive();                                                \
        break;                                                                 \
    case prot::cmd::SEND_END_##name:                                           \
        opool_n.deleteInactive();                                              \
        break;                                                                 \
    case prot::cmd::SEND_DATA_##name:                                          \
        handleSendOpool(cmddes,                                                \
                        dataEndPos,                                            \
                        junkSize,                                              \
                        [this](world::Sector* sector,                          \
                               bitsery::Deserializer<InputAdapter>& cmddes)    \
                        { block });                                            \
        break;

namespace sphyc
{

Model::Model(ui::UserInterface* userInterface,
             cfg::ConfigManager& config,
             mod::ModManager* modManager,
             gfx::RenderEngine* renderer,
             std::function<void(void)> afterLoadWorldClb)
    : userInterface(userInterface), config(config), modManager(modManager),
      renderer(renderer), afterLoadWorldClb(afterLoadWorldClb),
      clientRegistry(sendQueue)
{
    lastTSync = tim::getCurrentTimeU();
    assetFactory.componentFactory.registerAllComponents();
    lastGetAabbTree = tim::nowU();

    intFastCliServ =
        CFG_UINT(config, 100.0f, "net", "dump-int", "fast-cli-serv");
    realtimeDelay = 1000U * CFG_UINT(config, 100.0f, "net", "realtime-delay");
    mapDelay = 1000U * CFG_UINT(config, 2000.0f, "net", "map-delay");

    registerConnectSequence();
}

Model::~Model() {}

namespace
{

void drainQueue(moodycamel::ConcurrentQueue<net::CmdQueueData>& queue)
{
    net::CmdQueueData item;
    while (queue.try_dequeue(item))
    {
    }
}

}  // namespace

void Model::prepareForConnect()
{
    drainQueue(sendQueue);
    drainQueue(receiveQueue);
    loadWorldSequence.reset();
    timeSyncData.waiting = false;
    timeSyncData.cnt = 0;
    clientRegistry.clearSession();
    selectedEntities.clear();
    aabbs.clear();
    thirdPersonControl = def::ThirdPersonControl{};
}

void Model::modelLoop(float dt, long frametime)
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
            userInterface->setupViewModeUi(gfx::GameViewMode::Connecting);
            gameState = ClientGameState::LoadWorld;
            break;
        case ClientGameState::LoadWorld:

            if (loadWorldSequence.done())
            {
                LG_I("Exchanging world info with server done");
                afterLoadWorldClb();
                notifyReady();
                userInterface->setupViewModeUi(gfx::GameViewMode::ThirdPerson);
            }
            else
            {
                net::Exchange& curr = loadWorldSequence.getCurrentExchange();
                connectingData.status = curr.status;
                connectingData.info =
                    curr.infoGenerator ? curr.infoGenerator() : "";
                rmlModelConnecting.DirtyVariable("status");
                rmlModelConnecting.DirtyVariable("info");
            }
            break;
        case ClientGameState::NotifyServerReady:
            break;
        case ClientGameState::GameLoop:
            modelLoopGame(dt, frametime);
            break;
        case ClientGameState::ModdingTools:
        case ClientGameState::AtlasDebug:
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

void Model::modelLoopGame(float dt, long frametime)
{
    tim::Timepoint now = tim::getCurrentTimeU();
    static tim::Timepoint testTime = tim::getCurrentTimeU();
    static tim::Timepoint lastReqAllComponents = tim::getCurrentTimeU();
    static tim::Timepoint lastGetAabbTree = tim::getCurrentTimeU();
    static tim::Timepoint lastFastCliServ = tim::getCurrentTimeU();

    if (renderer->getViewMode() == gfx::GameViewMode::ThirdPerson
        || renderer->getViewMode() == gfx::GameViewMode::TacticalMap)
    {
        game_entity activeEntity = getActiveEntity();
        auto& reg = clientRegistry.getRegistry();
        if (reg.valid(activeEntity))
        {
            auto* trHist = reg.try_get<TransformHist>(activeEntity);
            if (trHist)
            {
                long rendertime =
                    frametime - timeSyncData.serverLatency - realtimeDelay;
                sphyc::ClientTransform tr;
                if (!trHist->interpolate(rendertime, tr, {.world = &world}))
                {
                    tr = trHist->latest();
                }
                auto sectorCoords = world.idToSectorCoords(tr.sectorId);
                if (renderer->getViewMode() == gfx::GameViewMode::TacticalMap)
                {
                    renderer->setActiveSector(sectorCoords.x, sectorCoords.y);
                }
                else
                {
                    renderer->panWorldTo(def::SectorCoords{
                        .pos = sectorCoords,
                        .sectorPos = tr.tr.pos,
                    });
                }
            }
        }
    }

    DO_PERIODIC_EXTNOW(lastFastCliServ,
                       intFastCliServ,
                       now,
                       [this]() { fastClientToServerUpdate(); });

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
                       [this]() { reqAllComponents(clientInfo.activeEntity); });
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
            parseCommand(
                cmddes, cmdData.sendType, cmd, flags, dataStartPos + len);
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

void Model::centerViewOnPlayer()
{
    if (renderer->getViewMode() == gfx::GameViewMode::StrategicMap
        || renderer->getViewMode() == gfx::GameViewMode::TacticalMap)
    {
        game_entity activeEntity = getActiveEntity();
        auto& reg = clientRegistry.getRegistry();
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
}

void Model::parseCommand(bitsery::Deserializer<InputAdapter>& cmddes,
                         net::SendType sendType,
                         uint16_t cmd,
                         uint8_t flags,
                         size_t dataEndPos)
{
    prot::cmd::State result = prot::cmd::State::SUCCESS;

    switch (cmd)
    {
        case prot::cmd::LOG:
        {
            std::string str;
            cmddes.text1b(str, dataEndPos - cmddes.adapter().currentReadPos());
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
                        timeSyncData.serverOffset = offsMin;
                        timeSyncData.serverLatency = latMin;
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
                world.createFromServer(worldShape, nullptr);
            }
            break;
        }
        case prot::cmd::CLIENT_INFO:
        {
            if (flags & CMD_FLAG_RESP && sendType == net::SendType::TCP)
            {
                cmddes.object(clientInfo);
            }
            break;
        }
        case prot::cmd::NOTIFY_CLIENT_READY:
        {
            if (flags & CMD_FLAG_RESP && sendType == net::SendType::TCP)
            {
                LG_I("Server accepted client readyness");
                gameState = ClientGameState::GameLoop;
                renderer->startGame();
            }
            break;
        }
        case prot::cmd::CONSOLE_CMD:
        {
            if (flags & CMD_FLAG_RESP)
            {
                std::string str;
                cmddes.text1b(str,
                              dataEndPos - cmddes.adapter().currentReadPos());
                LG_I("Console cmd response: {}", str);
                userInterface->addSystemMessage(str);
            }
            break;
        }
        case prot::cmd::SLOW_DUMP:
        {
            if ((flags & CMD_FLAG_RESP) == 0)
            {
                handleSlowDump(cmddes, dataEndPos);
            }
            break;
        }
        case prot::cmd::REQ_ALL_COMPONENTS:
        {
            if (flags & CMD_FLAG_RESP)
            {
                handleReqAllComponentsResp(cmddes, dataEndPos);
            }
            break;
        }
        case prot::cmd::DBG_GET_AABB_TREE:
        {
            if (flags & CMD_FLAG_RESP)
            {
                handleGetAabbTreeResp(cmddes, dataEndPos);
            }
            break;
        }
        case prot::cmd::ACTIVE_ENTITY_SWITCHED:
        {
            if ((flags & CMD_FLAG_RESP) == 0)
            {
                handleActiveEntitySwitched(cmddes, dataEndPos);
            }
            break;
        }
        case prot::cmd::ACTIVE_SECTOR_UPDATE:
        {
            if ((flags & CMD_FLAG_RESP) == 0)
            {
                handleActiveSectorDump(cmddes, dataEndPos);
            }
            break;
        }
        case prot::cmd::TOTAL_NUM_ENTITIES:
        {
            cmddes.value4b(clientRegistry.numServerEntities);
            break;
        }
        case prot::cmd::DESTROY_ENTITY:
        {
            handleDestroyEntity(cmddes, dataEndPos);
            break;
        }
        case prot::cmd::ACK_WORKSEQUENCER:
        {
            if ((flags & CMD_FLAG_RESP) == 0)
            {
                prot::MsgComposer mcomp(net::SendType::TCP, nullptr);
                mcomp.startCommand(prot::cmd::ACK_WORKSEQUENCER, CMD_FLAG_RESP);
                mcomp.execute(sendQueue);
            }
            break;
        }
        case prot::cmd::SEND_DATA_ITEM:
        {
            handleSendOpool(
                cmddes,
                dataEndPos,
                20 + 6,
                [this](world::Sector* sector,
                       bitsery::Deserializer<InputAdapter>& cmddes,
                       long frametime)
                {
                    GenericHandle32 handle;
                    ecs::Transform transform;
                    GenericHandle item;
                    uint32_t quantity;
                    cmddes.object(handle);
                    cmddes.object(transform);
                    cmddes.object(item);
                    cmddes.value4b(quantity);
                    sector->items.updateObject(
                        handle,
                        opool::ItemClient::Params{.transform = transform,
                                                  .item = item,
                                                  .quantity = quantity,
                                                  .time = frametime});
                });
            break;
        }
        case prot::cmd::SEND_DATA_PROJ:
        {
            handleSendOpool(
                cmddes,
                dataEndPos,
                16 + 6,
                [this](world::Sector* sector,
                       bitsery::Deserializer<InputAdapter>& cmddes,
                       long frametime)
                {
                    GenericHandle32 handle;
                    ecs::Transform transform;
                    GenericHandle projectile;
                    cmddes.object(handle);
                    cmddes.object(transform);
                    cmddes.object(projectile);
                    sector->projectiles.updateObject(
                        handle,
                        opool::ProjClient::Params{.tr = transform,
                                                  .time = frametime,
                                                  .proj = projectile});
                });
            break;
        }
        case prot::cmd::SEND_DATA_BEAM:
        {
            handleSendOpool(cmddes,
                            dataEndPos,
                            20 + 6,
                            [this](world::Sector* sector,
                                   bitsery::Deserializer<InputAdapter>& cmddes,
                                   long frametime)
                            {
                                GenericHandle32 handle;
                                vec2 p1;
                                vec2 p2;
                                GenericHandle beam;
                                cmddes.object(handle);
                                cmddes.object(p1);
                                cmddes.object(p2);
                                cmddes.object(beam);
                                sector->beams.updateObject(
                                    handle,
                                    opool::BeamClient::Params{.p1 = p1,
                                                              .p2 = p2,
                                                              .time = frametime,
                                                              .beam = beam});
                            });
            break;
        }
        case prot::cmd::UPD_ECS_REALTIME:
            handleEcsRealtime(cmddes, dataEndPos);
            break;
        case prot::cmd::UPD_ECS_MAP:
            handleEcsMap(cmddes, dataEndPos);
            break;
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
    // world.drawDebug(renderer, zoom);
    // auto& reg = clientRegistry.getRegistry();
    // reg.view<ecs::Transform, ecs::SectorId>().each(
    //     [this, &renderer](ecs::Transform& transform, ecs::SectorId& sectorId)
    //     {
    //         glm::vec2 worldPos =
    //             world.getWorldPosSectorOffset(sectorId.id,
    //                                           renderer.getSectorOffsetX(),
    //                                           renderer.getSectorOffsetY())
    //             + transform.pos;
    //         renderer.drawEllipse(worldPos,
    //                              glm::vec2(10.0f, 5.0f),
    //                              0xffffffff,
    //                              2.0f,
    //                              transform.rot,
    //                              0);
    //     });
}

void Model::drawTacticalMap(gfx::RenderEngine& renderer,
                            const glm::vec4& viewRect,
                            float zoom)
{
    // world.drawTacticalMap(renderer, viewRect, zoom);
    // uint32_t activeSectorId = getActiveSectorId();
    // drawRealtime(renderer);
    // auto& reg = clientRegistry.getRegistry();
    // for (const auto& entityId : selectedEntities)
    // {
    //     game_entity entity = clientRegistry.getEntity(entityId);
    //     if (reg.valid(entity))
    //     {
    //         auto* trans = reg.try_get<ecs::Transform>(entity);
    //         auto* sectorId = reg.try_get<ecs::SectorId>(entity);
    //         if (trans && sectorId && sectorId->id == activeSectorId)
    //         {
    //             glm::vec2 worldPos =
    //                 world.getWorldPosSectorOffset(sectorId->id,
    //                                               renderer.getSectorOffsetX(),
    //                                               renderer.getSectorOffsetY())
    //                 + trans->pos;
    //             renderer.drawShapeRectangle(worldPos,
    //                                         glm::vec2(40.0f, 40.0f),
    //                                         0xff004000,
    //                                         1.0f / zoom,
    //                                         0.0f,
    //                                         0);
    //         }
    //     }
    // }
    // if (overlayAabbTreeEnabled)
    // {
    //     drawOverlayAABBs(renderer, zoom);
    // }
}

void Model::drawMap(gfx::RenderEngine& renderer)
{
    long frametime = tim::nowU();
    long rendertime = frametime - timeSyncData.serverLatency - mapDelay;
    std::vector<RealtimeDrawBounds> bounds;
    createDrawBounds(bounds);
    drawMapIcons(renderer, bounds, rendertime);

    //world.drawStrategicMap(renderer, viewRect, zoom);


    // todo: Group entities by Pos and only show lists or fleets or groups
    // auto& reg = clientRegistry.getRegistry();
    // reg.view<ecs::Transform, ecs::SectorId, ecs::MapIcon>().each(
    //     [this, &renderer, &viewRect, zoom](ecs::Transform& transform,
    //                                        ecs::SectorId& sectorId,
    //                                        ecs::MapIcon& mapIcon)
    //     {
    //         glm::vec2 worldPos =
    //             world.getWorldPosSectorOffset(sectorId.id,
    //                                           renderer.getSectorOffsetX(),
    //                                           renderer.getSectorOffsetY())
    //             + transform.pos;
    //         if (smath::pointInsideRect(worldPos, viewRect))
    //         {
    //             auto* mapIconItem = modManager->getMapIconLib().getItem(
    //                 gobj::MapIconHandle(mapIcon.mapIconHandle));
    //             if (mapIconItem)
    //             {
    //                 mod::MappedTextureHandle mTexHandle =
    //                     *(mod::MappedTextureHandle*)&mapIconItem->texHandle;
    //                 const mod::MappedTexture* mappedTexture =
    //                     modManager->getResourceMap().getMappedTexture(
    //                         mTexHandle);
    //                 gfx::TextureHandle texHandle =
    //                     gfx::TextureHandle::Invalid();
    //                 if (mappedTexture)
    //                 {
    //                     texHandle = mappedTexture->texHandle;
    //                 }
    //                 renderer.queueTexRect(worldPos,
    //                                       glm::vec2(mapIconItem->size.x /
    //                                       zoom,
    //                                                 mapIconItem->size.y /
    //                                                 zoom),
    //                                       texHandle,
    //                                       transform.rot,
    //                                       gfx::RenderEngine::zIdxMapIconHull,
    //                                       0xff0010ff);
    //             }
    //         }
    //     });

    // for (const auto& entityId : selectedEntities)
    // {
    //     game_entity entity = clientRegistry.getEntity(entityId);
    //     if (reg.valid(entity))
    //     {
    //         auto* trans = reg.try_get<ecs::Transform>(entity);
    //         auto* sectorId = reg.try_get<ecs::SectorId>(entity);
    //         auto* mapIcon = reg.try_get<ecs::MapIcon>(entity);
    //         if (trans && sectorId && mapIcon)
    //         {
    //             auto* mapIconItem = modManager->getMapIconLib().getItem(
    //                 gobj::MapIconHandle(mapIcon->mapIconHandle));
    //             if (mapIconItem)
    //             {
    //                 glm::vec2 worldPos = world.getWorldPosSectorOffset(
    //                                          sectorId->id,
    //                                          renderer.getSectorOffsetX(),
    //                                          renderer.getSectorOffsetY())
    //                                      + trans->pos;
    //                 renderer.drawShapeRectangle(
    //                     worldPos,
    //                     glm::vec2(mapIconItem->size.x * 1.5f / zoom,
    //                               mapIconItem->size.y * 1.5f / zoom),
    //                     0xff004000,
    //                     1.0f / zoom,
    //                     0.0f,
    //                     0);
    //             }
    //         }
    //     }
    // }

    // if (overlayAabbTreeEnabled)
    // {
    //     drawOverlayAABBs(renderer, zoom);
    // }
}


void Model::drawMapIcons(gfx::RenderEngine& renderer,
                         const vector<RealtimeDrawBounds>& drawBounds,
                         long rendertime)
{
    float zoom = renderer.getWorldZoom();
    auto& reg = clientRegistry.getRegistry();
    reg.view<TransformHist, ecs::SectorId, ecs::MapIcon>().each(
        [this, &renderer, &reg, &drawBounds, rendertime, zoom](
            game_entity entity,
            TransformHist& tr,
            ecs::SectorId& sectorId,
            ecs::MapIcon& mapIcon)
        {
            // Check if in any visible sector
            for (auto& bounds : drawBounds)
            {
                if (bounds.sectorId != sectorId.id)
                {
                    continue;
                }
                ClientTransform clitr;
                if (tr.interpolate(rendertime, clitr, {.world = &world})
                    || tr.interpolate(
                        rendertime, clitr, {.world = &world}, true))
                {
                    if (bounds.aabb.containsPoint(clitr.tr.pos))
                    {
                        auto* mapIconItem = modManager->getMapIconLib().getItem(
                            gobj::MapIconHandle(mapIcon.mapIconHandle));
                        if (mapIconItem)
                        {
                            glm::vec2 worldPos =
                                world.getWorldPosSectorOffset(
                                    sectorId.id,
                                    renderer.getSectorOffsetX(),
                                    renderer.getSectorOffsetY())
                                + clitr.tr.pos;
                            mod::MappedTextureHandle mTexHandle =
                                *(mod::MappedTextureHandle*)&mapIconItem
                                     ->texHandle;
                            const mod::MappedTexture* mappedTexture =
                                modManager->getResourceMap().getMappedTexture(
                                    mTexHandle);
                            gfx::TextureHandle texHandle =
                                gfx::TextureHandle::Invalid();
                            if (mappedTexture)
                            {
                                texHandle = mappedTexture->texHandle;
                            }
                            renderer.queueTexRect(
                                worldPos,
                                glm::vec2(mapIconItem->size.x / zoom,
                                          mapIconItem->size.y / zoom),
                                texHandle,
                                clitr.tr.rot,
                                gfx::RenderEngine::zIdxMapIconHull,
                                0xff0010ff);
                        }
                    }
                }
            }
        });
}

void Model::drawRealtime(gfx::RenderEngine& renderer)
{
    long frametime = tim::nowU();
    long renderTime = frametime - timeSyncData.serverLatency - realtimeDelay;
    std::vector<RealtimeDrawBounds> bounds;
    createDrawBounds(bounds);
    drawRealtimeShips(renderer, bounds, renderTime);
    drawRealtimeAsteroids(renderer, bounds, renderTime);
    drawRealtimeProjectiles(renderer, bounds, renderTime);
    drawRealtimeBeams(renderer, bounds, renderTime);
    drawRealtimeItems(renderer, bounds, renderTime);
}

void Model::createDrawBounds(vector<RealtimeDrawBounds>& bounds)
{
    const auto& viewRect = clientInfo.clientViewRect;
    const auto& tl = viewRect.tl;
    const auto& br = viewRect.br;
    const float halfSize = world.getWorldShape().sectorSize / 2.0f;
    for (uint32_t secX = tl.pos.x; secX <= br.pos.x; ++secX)
    {
        for (uint32_t secY = tl.pos.y; secY <= br.pos.y; ++secY)
        {
            auto sector = world.getSectorByCoords(secX, secY);
            if (sector)
            {
                const vec2 lower(
                    (secX == tl.pos.x) ? tl.sectorPos.x : -halfSize,
                    (secY == tl.pos.y) ? tl.sectorPos.y : -halfSize);
                const vec2 upper((secX == br.pos.x) ? br.sectorPos.x : halfSize,
                                 (secY == br.pos.y) ? br.sectorPos.y
                                                    : halfSize);
                bounds.push_back({.sectorId = sector->getId(),
                                  .aabb = {.lower = lower, .upper = upper}});
            }
        }
    }
}

void Model::drawRealtimeShips(gfx::RenderEngine& renderer,
                              const vector<RealtimeDrawBounds>& drawBounds,
                              long rendertime)
{
    auto& reg = clientRegistry.getRegistry();
    reg.view<TransformHist,
             ecs::SectorId,
             ecs::Textures,
             ecs::Hull,
             ecs::Collider>()
        .each(
            [this, &renderer, &reg, &drawBounds, rendertime](
                game_entity entity,
                TransformHist& tr,
                ecs::SectorId& sectorId,
                ecs::Textures& textures,
                ecs::Hull& hull,
                ecs::Collider& coll)
            {
                // Check if in any visible sector
                for (auto& bounds : drawBounds)
                {
                    if (bounds.sectorId != sectorId.id)
                    {
                        continue;
                    }
                    // Check if collider intersects view rect
                    auto collider = modManager->getColliderLib().getItem(
                        coll.colliderHandle);
                    if (!collider)
                    {
                        break;
                    }
                    const float centerDist = collider->getSimpleMaxDist();
                    const vec2 centerDistVec = vec2(centerDist, centerDist);
                    sphyc::ClientTransform clitr;
                    if (tr.interpolate(rendertime, clitr, {.world = &world}))
                    {
                        const auto& trInt = clitr.tr;
                        const con::AABB aabb{.lower = trInt.pos - centerDistVec,
                                             .upper =
                                                 trInt.pos + centerDistVec};
                        if (!bounds.aabb.overlaps(aabb))
                        {
                            break;
                        }
                        // Do additional fine grained check
                        std::vector<vec2> w1;
                        sat2d::translateVertices(
                            collider->vertices, w1, trInt.pos, trInt.rot);
                        con::AABB fineAabb = ecs::calculateAABB(
                            trInt,
                            ecs::TransformCache{.c = cosf(trInt.rot),
                                                .s = sinf(trInt.rot)},
                            collider);
                        if (!bounds.aabb.overlaps(fineAabb))
                        {
                            break;
                        }
                        // draw Ship
                        glm::vec2 worldPos = world.getWorldPosSectorOffset(
                                                 sectorId.id,
                                                 renderer.getSectorOffsetX(),
                                                 renderer.getSectorOffsetY())
                                             + trInt.pos;
                        drawModuleTextures(renderer,
                                           trInt,
                                           gfx::RenderEngine::zIdxShipHull,
                                           hull,
                                           worldPos);
                        drawTextures(renderer,
                                     textures,
                                     trInt.rot,
                                     gfx::RenderEngine::zIdxShipHull,
                                     worldPos);
                        break;
                    }
                }
            });
}

// void Model::drawRealtimeStations(gfx::RenderEngine& renderer,
//                                  const vector<RealtimeDrawBounds>&
//                                  drawBounds)
// {
//     auto& reg = clientRegistry.getRegistry();
//     reg.view<ecs::Transform, ecs::SectorId, ecs::Station>().each(
//         [this, &renderer, &viewRect, &reg, activeSectorId](
//             ecs::Transform& transform,
//             ecs::SectorId& sectorId,
//             ecs::Station& station)
//         {
//             bool sectorFilter = activeSectorId == world::INVALID_SECTOR_ID
//                                 || sectorId.id == activeSectorId;
//             if (sectorFilter)
//             {
//                 glm::vec2 sectorOffset =
//                     world.getWorldPosSectorOffset(sectorId.id,
//                                                   renderer.getSectorOffsetX(),
//                                                   renderer.getSectorOffsetY());
//                 if (smath::pointInsideRect(sectorOffset + transform.pos,
//                                            viewRect))
//                 {
//                     drawStationTextures(
//                         renderer, transform, station, sectorOffset);
//                 }
//             }
//         });
// }

void Model::drawRealtimeAsteroids(gfx::RenderEngine& renderer,
                                  const vector<RealtimeDrawBounds>& drawBounds,
                                  long rendertime)
{
    auto& reg = clientRegistry.getRegistry();
    reg.view<TransformHist,
             ecs::SectorId,
             ecs::Asteroid,
             ecs::Textures,
             ecs::Collider>()
        .each(
            [this, &renderer, &reg, &drawBounds, rendertime](
                TransformHist& tr,
                ecs::SectorId& sectorId,
                ecs::Asteroid& asteroid,
                ecs::Textures& textures,
                ecs::Collider& coll)
            {
                // Check if in any visible sector
                for (auto& bounds : drawBounds)
                {
                    if (bounds.sectorId != sectorId.id)
                    {
                        continue;
                    }
                    // Check if collider intersects view rect
                    auto collider = modManager->getColliderLib().getItem(
                        coll.colliderHandle);
                    if (!collider)
                    {
                        break;
                    }
                    const float centerDist = collider->getSimpleMaxDist();
                    const vec2 centerDistVec = vec2(centerDist, centerDist);
                    sphyc::ClientTransform clitr;
                    if (tr.interpolate(rendertime, clitr, {.world = &world}))
                    {
                        const auto& trInt = clitr.tr;
                        const con::AABB aabb{.lower = trInt.pos - centerDistVec,
                                             .upper =
                                                 trInt.pos + centerDistVec};
                        if (!bounds.aabb.overlaps(aabb))
                        {
                            break;
                        }
                        // Do additional fine grained check
                        std::vector<vec2> w1;
                        sat2d::translateVertices(
                            collider->vertices, w1, trInt.pos, trInt.rot);
                        con::AABB fineAabb = ecs::calculateAABB(
                            trInt,
                            ecs::TransformCache{.c = cosf(trInt.rot),
                                                .s = sinf(trInt.rot)},
                            collider);
                        if (!bounds.aabb.overlaps(fineAabb))
                        {
                            break;
                        }
                        glm::vec2 worldPos = world.getWorldPosSectorOffset(
                                                 sectorId.id,
                                                 renderer.getSectorOffsetX(),
                                                 renderer.getSectorOffsetY())
                                             + trInt.pos;
                        drawTextures(renderer,
                                     textures,
                                     trInt.rot,
                                     gfx::RenderEngine::zIdxAsteroid,
                                     worldPos);
                    }
                }
            });
}

void Model::drawRealtimeItems(gfx::RenderEngine& renderer,
                              const vector<RealtimeDrawBounds>& drawBounds,
                              long rendertime)
{
    for (auto& bound : drawBounds)
    {
        auto sector = world.getSector(bound.sectorId);
        if (!sector)
        {
            continue;
        }
        const con::AABB visibleBounds = {
            .lower = bound.aabb.lower - vec2(100.0f, 100.0f),
            .upper = bound.aabb.upper + vec2(100.0f, 100.0f),
        };
        sector->items.foreach (
            [&renderer, &visibleBounds, this, rendertime, &bound](
                opool::ItemClient& item)
            {
                opool::vec2Mixer posMix;
                if (item.pos.interpolate(rendertime, posMix, {})
                    && visibleBounds.containsPoint(posMix.pos))
                {
                    auto itemData = modManager->getItemLib().getItem(item.item);
                    if (itemData)
                    {
                        glm::vec2 worldPos = world.getWorldPosSectorOffset(
                                                 bound.sectorId,
                                                 renderer.getSectorOffsetX(),
                                                 renderer.getSectorOffsetY())
                                             + posMix.pos;
                        drawTexture(renderer,
                                    itemData->worldTexture,
                                    item.rot,
                                    vec2{0.0f, 0.0f},
                                    gfx::RenderEngine::zIdxItem,
                                    worldPos,
                                    true);
                    }
                    return true;
                }
                else
                {
                    return false;
                }
            });
    }
}

void Model::drawRealtimeProjectiles(
    gfx::RenderEngine& renderer,
    const vector<RealtimeDrawBounds>& drawBounds,
    long rendertime)
{
    for (auto& bound : drawBounds)
    {
        auto sector = world.getSector(bound.sectorId);
        if (!sector)
        {
            continue;
        }
        const con::AABB visibleBounds = {
            .lower = bound.aabb.lower - vec2(100.0f, 100.0f),
            .upper = bound.aabb.upper + vec2(100.0f, 100.0f),
        };
        sector->projectiles.foreach (
            [&renderer, &visibleBounds, this, rendertime, &bound](
                opool::ProjClient& proj)
            {
                opool::vec2Mixer posMix;
                if (proj.pos.interpolate(rendertime, posMix, {})
                    && visibleBounds.containsPoint(posMix.pos))
                {
                    auto projectile =
                        modManager->getProjectileLib().getItem(proj.proj);
                    if (projectile)
                    {
                        glm::vec2 worldPos = world.getWorldPosSectorOffset(
                                                 bound.sectorId,
                                                 renderer.getSectorOffsetX(),
                                                 renderer.getSectorOffsetY())
                                             + posMix.pos;
                        drawTextures(renderer,
                                     projectile->textures,
                                     proj.rot,
                                     gfx::RenderEngine::zIdxProjectile,
                                     worldPos);
                    }
                    return true;
                }
                else
                {
                    return false;
                }
            });
    }
}

void Model::drawRealtimeBeams(gfx::RenderEngine& renderer,
                              const vector<RealtimeDrawBounds>& drawBounds,
                              long rendertime)
{
    for (auto& bound : drawBounds)
    {
        auto sector = world.getSector(bound.sectorId);
        if (!sector)
        {
            continue;
        }
        const con::AABB visibleBounds = {
            .lower = bound.aabb.lower - vec2(100.0f, 100.0f),
            .upper = bound.aabb.upper + vec2(100.0f, 100.0f),
        };
        sector->beams.foreach (
            [&renderer, &visibleBounds, this, rendertime, &bound](
                opool::BeamClient& beam)
            {
                opool::LineMixer lineMix;
                if (beam.line.interpolate(rendertime, lineMix, {}))
                {
                    const vec2 pos1 = lineMix.pos1;
                    const vec2 pos2 = lineMix.pos2;
                    const vec2 aa = vec2(std::min(pos1.x, pos2.x),
                                         std::min(pos1.y, pos2.y));
                    const vec2 bb = vec2(std::max(pos1.x, pos2.x),
                                         std::max(pos1.y, pos2.y));
                    const con::AABB beamAabb = {.lower = aa, .upper = bb};
                    if (visibleBounds.overlaps(beamAabb))
                    {
                        auto beamD =
                            modManager->getBeamLib().getItem(beam.beam);
                        if (beamD)
                        {
                            glm::vec2 worldPosOffset =
                                world.getWorldPosSectorOffset(
                                    bound.sectorId,
                                    renderer.getSectorOffsetX(),
                                    renderer.getSectorOffsetY());
                            renderer.drawLine(
                                worldPosOffset + lineMix.pos1,
                                worldPosOffset + lineMix.pos2,
                                beamD->color,
                                beamD->width,
                                gfx::RenderEngine::zIdxProjectile);
                        }
                        return true;
                    }
                }
                return false;
            });
    }
}


void Model::drawStationTextures(gfx::RenderEngine& renderer,
                                const ecs::Transform& parentTransform,
                                ecs::Station& station,
                                const glm::vec2& sectorOffset)
{
    auto& reg = clientRegistry.getRegistry();
    for (auto& stationPartRef : station.stationParts)
    {
        game_entity stationPartEntity =
            clientRegistry.getEntity(stationPartRef.entityId);
        if (stationPartEntity != entt::null)
        {
            auto* stationPartTextures =
                reg.try_get<ecs::Textures>(stationPartEntity);
            auto* stationPartTransform =
                reg.try_get<ecs::Transform>(stationPartEntity);
            if (stationPartTextures && stationPartTransform)
            {
                drawTextures(renderer,
                             *stationPartTextures,
                             stationPartTransform->rot,
                             gfx::RenderEngine::zIdxStation,
                             sectorOffset + stationPartTransform->pos);
            }
        }
    }
}

void Model::drawModuleTextures(gfx::RenderEngine& renderer,
                               const ecs::Transform& parentTransform,
                               const int8_t parentZ,
                               ecs::Hull& hull,
                               const glm::vec2& worldPos)
{
    auto& reg = clientRegistry.getRegistry();
    for (auto& modRef : hull.modules)
    {
        game_entity moduleEntity = clientRegistry.getEntity(modRef.entityId);
        if (moduleEntity != entt::null)
        {
            const int8_t moduleOffset =
                gobj::ModuleSlotZOffset[static_cast<uint8_t>(
                    modRef.moduleSlotType)];
            const int8_t moduleZ = parentZ + moduleOffset;
            auto* anchorFixed = reg.try_get<ecs::AnchorFixed>(moduleEntity);
            auto* moduleTextures = reg.try_get<ecs::Textures>(moduleEntity);
            auto* module = reg.try_get<ecs::Module>(moduleEntity);
            if (anchorFixed && moduleTextures && module)
            {
                gobj::Module* moduleItem = modManager->getModuleLib().getItem(
                    gobj::ModuleHandle(module->moduleHandle));
                float moduleRot = 0.0f;
                if (moduleItem)
                {
                    switch (moduleItem->type)
                    {
                        case gobj::ModuleType::Turret:
                        {
                            auto* turret =
                                reg.try_get<ecs::Turret>(moduleEntity);
                            if (turret)
                            {
                                moduleRot = turret->currentAngle;
                            }
                        }
                        break;
                        default:
                            moduleRot = 0.0f;
                            break;
                    }
                }
                else
                {
                    moduleRot = 0.0f;
                }
                const vec2 anchorFixedPos =
                    smath::rotateVec2(anchorFixed->pos, parentTransform.rot);
                drawTextures(renderer,
                             *moduleTextures,
                             parentTransform.rot + anchorFixed->rot + moduleRot,
                             moduleZ,
                             worldPos + anchorFixedPos);
            }
        }
    }
}  // namespace sphyc

void Model::drawTexture(gfx::RenderEngine& renderer,
                        const GenericHandle texture,
                        float rot,
                        const vec2& size,
                        const int8_t parentZ,
                        const glm::vec2& worldPos,
                        bool useTexSize)
{
    const mod::MappedTexture* mappedTexture =
        modManager->getResourceMap().getMappedTexture(
            *(mod::MappedTextureHandle*)&texture);
    gfx::TextureHandle texHandle = gfx::TextureHandle::Invalid();
    if (mappedTexture)
    {
        texHandle = mappedTexture->texHandle;
    }
    vec2 mySize;
    if (useTexSize)
    {
        renderer.getTexturePixelSize(texHandle, mySize);
        mySize = mySize * gfx::kTexturePixelToWorld;
    }
    else
    {
        mySize = size;
    }
    renderer.queueTexRect(worldPos,
                          mySize,
                          texHandle,
                          rot,
                          gfx::RenderEngine::zIdxItem,
                          0xffffffff);
}

void Model::drawTextures(gfx::RenderEngine& renderer,
                         const ecs::Textures& textures,
                         float rot,
                         const int8_t parentZ,
                         const glm::vec2& worldPos)
{
    drawTextures(renderer, textures.texturesHandle, rot, parentZ, worldPos);
}

void Model::drawTextures(gfx::RenderEngine& renderer,
                         gobj::TexturesHandle texHandle,
                         float rot,
                         const int8_t parentZ,
                         const glm::vec2& worldPos)
{
    auto* texturesItem = modManager->getTexturesLib().getItem(texHandle);
    if (texturesItem)
    {
        for (const auto& texture : texturesItem->textures)
        {
            mod::MappedTextureHandle mTexHandle =
                *(mod::MappedTextureHandle*)&texture.texHandle;
            const mod::MappedTexture* mappedTexture =
                modManager->getResourceMap().getMappedTexture(mTexHandle);
            gfx::TextureHandle texHandleGFX = gfx::TextureHandle::Invalid();
            if (mappedTexture)
            {
                texHandleGFX = mappedTexture->texHandle;
            }
            // Offset is in body space; rotate by +rot (CW, Y-down).
            // drawTexRect uses (rot - texture.rot), same convention as
            // ModdingTools::drawTextures.
            vec2 texOffset = smath::rotateVec2(
                vec2(texture.bounds.x, texture.bounds.y), rot);
            renderer.queueTexRect(
                worldPos + texOffset,
                glm::vec2(texture.bounds.z, texture.bounds.w),
                texHandleGFX,
                rot - texture.rot,
                parentZ + texture.zOffset,
                0xffffffff,
                0,
                glm::vec2(texture.tileOffset.x, texture.tileOffset.y),
                glm::vec2(texture.tileCount.x, texture.tileCount.y));
        }
    }
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
    prepareForConnect();
    this->clientInfo = def::ClientInfo("", clientInfo, 0);
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
    prepareForConnect();
    switch (gameState)
    {
        case ClientGameState::Authenticating:
            LG_W("Authentication refused");
            gameState = ClientGameState::MainMenu;
            break;
        case ClientGameState::GameLoop:
        case ClientGameState::LoadWorld:
        case ClientGameState::NotifyServerReady:
        case ClientGameState::Authenticated:
        case ClientGameState::VersionCheck:
            LG_W("Disconnected from server");
            gameState = ClientGameState::MainMenu;
            break;
        default:
            break;
    }
}

void Model::handleSlowDump(bitsery::Deserializer<InputAdapter>& cmddes,
                           size_t dataEndPos)
{
    uint32_t compHash;
    cmddes.value4b(compHash);
    for (auto& [hash, helper] :
         assetFactory.componentFactory.getComponentHelpers())
    {
        if (hash == compHash)
        {
            while (cmddes.adapter().currentReadPos() < dataEndPos - 6)
            {
                uint32_t sectorId;
                uint16_t numEntities;
                cmddes.value4b(sectorId);
                cmddes.value2b(numEntities);
                for (uint i = 0; i < numEntities; ++i)
                {
                    ecs::EntityId entityId;
                    cmddes.object(entityId);
                    game_entity entity =
                        clientRegistry.enttFromServerId(entityId);
                    if (entity == entt::null)
                    {
                        continue;
                    }
                    auto& reg = clientRegistry.getRegistry();
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

void Model::handleActiveSectorDump(bitsery::Deserializer<InputAdapter>& cmddes,
                                   size_t dataEndPos)
{
    (void)dataEndPos;
    uint32_t compHash;
    cmddes.value4b(compHash);
    for (auto& [hash, helper] :
         assetFactory.componentFactory.getComponentHelpers())
    {
        if (hash == compHash)
        {
            uint32_t sectorId;
            uint16_t numEntities;
            cmddes.value4b(sectorId);
            cmddes.value2b(numEntities);
            for (uint i = 0; i < numEntities; ++i)
            {
                ecs::EntityId entityId;
                cmddes.object(entityId);
                game_entity entity = clientRegistry.enttFromServerId(entityId);
                auto& reg = clientRegistry.getRegistry();
                auto [x, y] = world.idToSectorCoords(sectorId);
                reg.emplace_or_replace<ecs::SectorId>(entity, sectorId, x, y);
                reg.emplace_or_replace<ecs::EntityId>(entity, entityId);
                helper.deserializeIntoRegistry(reg, entity, cmddes);
            }
        }
    }
}

void Model::handleReqAllComponentsResp(
    bitsery::Deserializer<InputAdapter>& cmddes,
    size_t dataEndPos)
{
    ecs::EntityId entityId;
    cmddes.object(entityId);
    game_entity entity = clientRegistry.enttFromServerId(entityId, false);
    if (entity == entt::null)
    {
        LG_W("Entity not found for component sync: {}", entityId);
        cmddes.adapter().currentReadPos(dataEndPos);
        return;
    }
    const auto& compHelpers =
        assetFactory.componentFactory.getComponentHelpers();
    while (cmddes.adapter().currentReadPos() <= dataEndPos - 4)
    {
        const size_t posBefore = cmddes.adapter().currentReadPos();
        uint32_t compHash;
        cmddes.value4b(compHash);
        auto it = compHelpers.find(compHash);
        if (it != compHelpers.end())
        {
            auto& reg = clientRegistry.getRegistry();
            it->second.deserializeIntoRegistry(reg, entity, cmddes);
            if (cmddes.adapter().currentReadPos() == posBefore)
            {
                LG_E(
                    "Component deserialize made no progress (hash={}), "
                    "aborting entity sync",
                    compHash);
                cmddes.adapter().currentReadPos(dataEndPos);
                return;
            }
        }
        else
        {
            LG_W("Unknown component hash: {}", compHash);
            cmddes.adapter().currentReadPos(dataEndPos);
            return;
        }
    }
}

void Model::toggleTacticalView()
{
    if (gameState == ClientGameState::GameLoop)
    {
        renderer->clbToggleTacticalView();
        userInterface->setupViewModeUi(renderer->getViewMode());
    }
}

void Model::toggleStrategicView()
{
    if (gameState == ClientGameState::GameLoop)
    {
        renderer->clbToggleStrategicView();
        userInterface->setupViewModeUi(renderer->getViewMode());
    }
}

void Model::gotoModdingTools()
{
    renderer->gotoModdingTools();
    userInterface->setupViewModeUi(gfx::GameViewMode::ModdingTools);
    gameState = ClientGameState::ModdingTools;
}

void Model::gotoAtlasDebug()
{
    userInterface->setupViewModeUi(gfx::GameViewMode::AtlasDebug);
    gameState = ClientGameState::AtlasDebug;
}

void Model::gotoMenu()
{
    userInterface->setupViewModeUi(gfx::GameViewMode::Menu);
    gameState = ClientGameState::MainMenu;
}

void Model::reqAllComponents(ecs::EntityId entityId)
{
    prot::MsgComposer mcomp(net::SendType::TCP, nullptr);
    mcomp.startCommand(prot::cmd::REQ_ALL_COMPONENTS, 0);
    mcomp.ser->object(entityId);
    mcomp.execute(sendQueue);
}

ecs::EntityId
Model::selectEntityAtWorldPos(const def::SectorCoords& sectorCoords)
{
    selectedEntities.clear();
    ecs::EntityId selectedEntity = ecs::EntityId::Invalid();
    auto& reg = clientRegistry.getRegistry();
    for (const auto entity : reg.view<ecs::SectorId,
                                      ecs::Transform,
                                      ecs::EntityId,
                                      ecs::Collider>())
    {
        auto& sid = reg.get<ecs::SectorId>(entity);
        auto& tr = reg.get<ecs::Transform>(entity);
        auto& eid = reg.get<ecs::EntityId>(entity);
        auto& collider = reg.get<ecs::Collider>(entity);
        if (sid.coord == sectorCoords.pos)
        {
            if (collider.isPointInsideWorld(sectorCoords.sectorPos,
                                            tr,
                                            std::cos(tr.rot),
                                            std::sin(tr.rot),
                                            &modManager->getColliderLib()))
            {
                selectedEntity = eid;
                selectedEntities.push_back(eid);
                break;
            }
        }
    }
    return selectedEntity;
}

ecs::EntityId
Model::selectEntityAtWorldPosFast(const def::SectorCoords& sectorCoords,
                                  float dist2)
{
    selectedEntities.clear();
    ecs::EntityId selectedEntity = ecs::EntityId::Invalid();
    auto& reg = clientRegistry.getRegistry();
    for (const auto entity : reg.view<ecs::SectorId,
                                      ecs::Transform,
                                      ecs::EntityId,
                                      ecs::tag::Selectable>())
    {
        auto& sid = reg.get<ecs::SectorId>(entity);
        auto& tr = reg.get<ecs::Transform>(entity);
        auto& eid = reg.get<ecs::EntityId>(entity);
        if (sid.coord == sectorCoords.pos)
        {
            if (glm::length2(tr.pos - sectorCoords.sectorPos) <= dist2)
            {
                selectedEntity = eid;
                selectedEntities.push_back(eid);
                break;
            }
        }
    }
    return selectedEntity;
}

void Model::selectEntitiesInsideRect(const def::SectorCoords& start,
                                     const def::SectorCoords& end)
{
    selectedEntities.clear();
    auto& reg = clientRegistry.getRegistry();
    auto& xMin = def::SectorCoords::minX(start, end);
    auto& xMax = def::SectorCoords::maxX(start, end);
    auto& yMin = def::SectorCoords::minY(start, end);
    auto& yMax = def::SectorCoords::maxY(start, end);
    reg.view<ecs::SectorId,
             ecs::Transform,
             ecs::EntityId,
             ecs::tag::Selectable>()
        .each(
            [this, &xMin, &xMax, &yMin, &yMax](
                ecs::SectorId& sid, ecs::Transform& tr, ecs::EntityId& eid)
            {
                bool xMinBool = sid.coord.x > xMin.pos.x
                                || (sid.coord.x == xMin.pos.x
                                    && tr.pos.x > xMin.sectorPos.x);
                bool xMaxBool = sid.coord.x < xMax.pos.x
                                || (sid.coord.x == xMax.pos.x
                                    && tr.pos.x < xMax.sectorPos.x);
                bool yMinBool = sid.coord.y > yMin.pos.y
                                || (sid.coord.y == yMin.pos.y
                                    && tr.pos.y > yMin.sectorPos.y);
                bool yMaxBool = sid.coord.y < yMax.pos.y
                                || (sid.coord.y == yMax.pos.y
                                    && tr.pos.y < yMax.sectorPos.y);
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

void Model::selectedEntitiesMoveCmd(def::SectorCoords& sectorCoords, bool queue)
{
    std::size_t idx = 0;
    if (selectedEntities.empty())
    {
        return;
    }
    prot::MsgComposer mcomp(net::SendType::TCP, nullptr);
    for (auto& entityId : selectedEntities)
    {
        mcomp.startCommand(prot::cmd::SEL_CMD_MOVETO, 0);
        mcomp.ser->object(entityId);
        mcomp.ser->object(sectorCoords);
        prot::cmd::MoveToFlags flags{.queue = queue};
        mcomp.ser->value1b(*((uint8_t*)&flags));
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
                                  size_t dataEndPos)
{
    (void)dataEndPos;
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
            renderer.drawShapeRectangle(
                pos, size, 0x10ffffff, 1.0f / zoom, 0.0f, 0);
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
    size_t dataEndPos)
{
    (void)dataEndPos;
    ecs::EntityId entityId;
    cmddes.object(entityId);
    clientInfo.activeEntity = entityId;
}

void Model::registerConnectSequence()
{
    loadWorldSequence.registerExchange(net::Exchange(
        prot::cmd::WORLD_INFO,
        []() {},
        []() {},
        [](bitsery::Serializer<OutputAdapter>&) {},
        "Discovering Galaxy Dimensions",
        []() { return "Where does the galaxy end?"; }));
    loadWorldSequence.registerExchange(net::Exchange(
        prot::cmd::CLIENT_INFO,
        []() {},
        []() {},
        [](bitsery::Serializer<OutputAdapter>&) {},
        "Synchronizing Commander",
        []() { return "Who am I?"; }));
    loadWorldSequence.registerExchange(net::Exchange(
        prot::cmd::ALL_ENTT_COMPONENTS,
        []() {},
        []() {},
        [](bitsery::Serializer<OutputAdapter>&) {},
        "Exploring Sectors",
        [this]()
        {
            return fmt::format("Loading {} of {} entities...",
                               clientRegistry.getNumClientEntities(),
                               clientRegistry.numServerEntities);
        }));
}

uint32_t Model::getActiveSectorId()
{
    game_entity activeEntity = getActiveEntity();
    auto& reg = clientRegistry.getRegistry();
    if (reg.valid(activeEntity))
    {
        auto* sectorId = reg.try_get<ecs::SectorId>(activeEntity);
        if (sectorId)
        {
            return sectorId->id;
        }
    }
    return 0;
}

game_entity Model::getActiveEntity()
{
    return clientRegistry.getEntity(clientInfo.activeEntity);
}

void Model::fastClientToServerUpdate()
{
    prot::MsgComposer mcomp(net::SendType::TCP, nullptr);

    if (renderer->getViewMode() == gfx::GameViewMode::ThirdPerson)
    {
        mcomp.startCommand(prot::cmd::THIRD_PERSON_CTRL, 0);
        mcomp.ser->object(thirdPersonControl);
        mcomp.finishCommand();
    }
    mcomp.startCommand(prot::cmd::CLIENT_VIEW_RECT, 0);
    mcomp.ser->object(clientInfo.clientViewRect);

    mcomp.execute(sendQueue);
}

// void Model::sendThirdPersonControl()
// {
//     prot::MsgComposer mcomp(net::SendType::TCP, nullptr);
//     mcomp.startCommand(prot::cmd::THIRD_PERSON_CTRL, 0);
//     mcomp.ser->object(thirdPersonControl);
//     mcomp.execute(sendQueue);
// }

void Model::setupDataModelConnecting()
{
    auto connectingConstructor = userInterface->getDataModel("connecting");
    if (connectingConstructor)
    {
        connectingConstructor.Bind("status", &connectingData.status);
        connectingConstructor.Bind("info", &connectingData.info);
    }
    rmlModelConnecting = connectingConstructor.GetModelHandle();
}

void Model::handleDestroyEntity(bitsery::Deserializer<InputAdapter>& cmddes,
                                size_t dataEndPos)
{
    (void)dataEndPos;
    ecs::EntityId entityId;
    cmddes.object(entityId);
    clientRegistry.destroyServerEntity(entityId);
}

void Model::handleSendOpool(
    bitsery::Deserializer<InputAdapter>& cmddes,
    size_t dataEndPos,
    size_t junkSize,
    std::function<void(world::Sector* sector,
                       bitsery::Deserializer<InputAdapter>& cmddes,
                       long time)> clb)
{
    uint32_t sectorId;
    long frametime;
    cmddes.value4b(sectorId);
    cmddes.value8b(frametime);
    auto sector = world.getSector(sectorId);
    if (!sector)
    {
        return;
    }
    while ((int)cmddes.adapter().currentReadPos()
           <= (int)(dataEndPos) - (int)junkSize)
    {
        clb(sector, cmddes, frametime);
    }
}

void Model::handleEcsRealtime(bitsery::Deserializer<InputAdapter>& cmddes,
                              size_t dataEndPos)
{
    uint32_t sectorId;
    long frametime;
    cmddes.value4b(sectorId);
    cmddes.value8b(frametime);
    auto& reg = clientRegistry.getRegistry();
    while ((int)cmddes.adapter().currentReadPos() < (int)(dataEndPos)-8)
    {
        namespace Rtf = prot::cmd::Rtf;
        ecs::EntityId entityId;
        Rtf::Flags flags;
        cmddes.object(entityId);
        cmddes.value2b(flags);
        game_entity entity = clientRegistry.enttFromServerId(entityId, false);
        auto sector = reg.try_get<ecs::SectorId>(entity);
        if (sector && sector->id != sectorId)
        {
            sector->id = sectorId;
            sector->coord = world.idToSectorCoords(sectorId);
        }
        if (flags & Rtf::HasTransform)
        {
            ecs::Transform tr;
            cmddes.object(tr);
            auto& clitr = reg.get_or_emplace<TransformHist>(entity);
            clitr.addSample({.tr = tr, .sectorId = sectorId}, frametime);
        }
        if (flags & Rtf::HasThrust)
        {
            vec2 thrust;
            cmddes.object(thrust);
            auto phythrust = reg.try_get<ecs::PhyThrust>(entity);
            if (phythrust)
            {
                phythrust->thrustLocal = thrust;
            }
        }
        if (flags & Rtf::HasTurret)
        {
            float rot;
            cmddes.value4b(rot);
            auto turr = reg.try_get<ecs::Turret>(entity);
            if (turr)
            {
                turr->currentAngle = rot;
            }
        }
    }
}


void Model::handleEcsMap(bitsery::Deserializer<InputAdapter>& cmddes,
                         size_t dataEndPos)
{
    uint32_t sectorId;
    long frametime;
    cmddes.value4b(sectorId);
    cmddes.value8b(frametime);
    auto& reg = clientRegistry.getRegistry();
    while ((int)cmddes.adapter().currentReadPos() < (int)(dataEndPos)-6)
    {
        ecs::EntityId entityId;
        cmddes.object(entityId);
        game_entity entity = clientRegistry.enttFromServerId(entityId, false);
        auto sector = reg.try_get<ecs::SectorId>(entity);
        if (sector && sector->id != sectorId)
        {
            sector->id = sectorId;
            sector->coord = world.idToSectorCoords(sectorId);
        }
        ecs::Transform tr;
        cmddes.object(tr);
        auto& clitr = reg.get_or_emplace<TransformHist>(entity);
        clitr.addSample(
            {.tr = tr, .sectorId = sectorId}, frametime, mapDelay / 2);
    }
}

}  // namespace sphyc