#include "aabb-tree.hpp"
#include "client-pool-obj.hpp"
#include "comp-phy.hpp"
#include "config-manager.hpp"
#include "lib-modules.hpp"
#include "lib-projectile.hpp"
#include "lib-textures.hpp"
#include "logging.hpp"
#include "net-shared.hpp"
#include "ptr-handle.hpp"
#include "render-engine.hpp"
#include "sector.hpp"
#include "std-inc.hpp"
#include <cmath>
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

static uint32_t factionColTest(GenericHandle handle)
{
    switch (handle.idx)
    {
        case 0:
            return 0xffff0000;
        case 1:
            return 0xff00ff00;
        default:
            return 0xff0000ff;
    }
}

Model::Model(ui::UserInterface* userInterface,
             cfg::ConfigManager& config,
             mod::ModManager* modManager,
             gfx::RenderEngine* renderer,
             std::function<void(void)> afterLoadWorldClb,
             ecs::PtrHandle* ptrHandle)
    : userInterface(userInterface), config(config), modManager(modManager),
      renderer(renderer), afterLoadWorldClb(afterLoadWorldClb),
      clientRegistry(sendQueue), ptrHandle(ptrHandle)
{
    assetFactory.componentFactory.registerAllComponents();
    lastGetAabbTree = tim::nowU();

    ptrHandle->model = this;
    ptrHandle->world = &world;

    intFastCliServ =
        CFG_UINT(config, 100.0f, "net", "dump-int", "fast-cli-serv");
    realtimeDelay = 1000U * CFG_UINT(config, 100.0f, "net", "realtime-delay");
    mapDelay = 1000U * CFG_UINT(config, 2000.0f, "net", "map-delay");

    registerConnectSequence();

    lastFastCliServ = tim::nowU();
    lastGetAabbTree = tim::nowU();
    lastReqAllComponents = tim::nowU();
    lastTSync = tim::nowU();
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
            modelLoopGame(dt);
            break;
        case ClientGameState::VersionCheck:
            break;
        case ClientGameState::Authenticated:
            loadWorldSequence.start(sendQueue);
            if (afterConnectState == AfterConnectState::Game)
            {
                userInterface->setupViewModeUi(gfx::GameViewMode::Connecting);
            }
            gameState = ClientGameState::LoadWorld;
            break;
        case ClientGameState::LoadWorld:

            if (loadWorldSequence.done())
            {
                LG_I("Exchanging world info with server done");
                afterLoadWorldClb();
                notifyReady();
                renderer->zoom(0.5f, {0.0, 0.0}, true);
                if (afterConnectState == AfterConnectState::Game)
                {
                    userInterface->setupViewModeUi(
                        gfx::GameViewMode::ThirdPerson);
                }
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
            modelLoopGame(dt);
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

void Model::setCurrentTime(gfx::RenderEngine& renderer, long frametime)
{
    this->frametime = frametime;
    long delay = renderer.getViewMode() == gfx::GameViewMode::ThirdPerson
                         || renderer.getWorldZoom() >= realtimeZoomThr
                     ? realtimeDelay
                     : mapDelay;
    rendertime = frametime - timeSyncData.serverLatency - delay;
}

void Model::modelLoopGame(float dt)
{
    if (renderer->getViewMode() == gfx::GameViewMode::ThirdPerson
        || renderer->getViewMode() == gfx::GameViewMode::Map)
    {
        game_entity activeEntity = getActiveEntity();
        auto& reg = clientRegistry.getRegistry();
        if (reg.valid(activeEntity))
        {
            auto* trHist = reg.try_get<TransformHist>(activeEntity);
            if (trHist)
            {
                sphyc::ClientTransform tr;
                if (!trHist->interpolate(rendertime, tr, {.world = &world}))
                {
                    tr = trHist->latest();
                }
                auto sectorCoords = world.idToSectorCoords(tr.sectorId);
                if (renderer->getViewMode() == gfx::GameViewMode::Map)
                {
                    renderer->setActiveSector(sectorCoords.x, sectorCoords.y);
                }
                else
                {
                    def::SectorPos old{(uint32_t)renderer->getSectorOffsetX(),
                                       (uint32_t)renderer->getSectorOffsetY()};
                    const vec2 oldPos = renderer->getWorldCameraPosition();
                    const auto secFrom = world.sectorCoordsToId(old);
                    const auto secTo = world.sectorCoordsToId(sectorCoords);
                    const auto newPos = tr.tr.pos;
                    if (secFrom == secTo)
                    {
                        const vec2 moveVec = newPos - oldPos;
                        const vec2 interpol = oldPos + 0.05f * moveVec;
                        if (glm::length2(moveVec) < 500.0f * 500.0f)
                        {
                            renderer->panWorldTo(def::SectorCoords{
                                .pos = old,
                                .sectorPos = interpol,
                            });
                        }
                        else
                        {
                            renderer->panWorldTo(def::SectorCoords{
                                .pos = sectorCoords,
                                .sectorPos = newPos,
                            });
                        }
                    }
                    else
                    {
                        // todo: make dt independent and implement smooth
                        // panning in render engine todo: this + model.hpp.
                        // shared function for sector pos interpolation
                        // todo: only do complex interpolation when id !=
                        // newId
                        const vec2 prevPosTr =
                            world.translateCoords(oldPos, secFrom, secTo);
                        const vec2 moveVec = newPos - prevPosTr;
                        if (glm::length2(moveVec) < 500.0f * 500.0f)
                        {
                            const vec2 interPos = oldPos + 0.05f * moveVec;
                            const def::SectorPos prevSectorXY = old;
                            const def::SectorCoords coordsTr =
                                world.translateOOBCoords(
                                    {.pos = prevSectorXY,
                                     .sectorPos = interPos});
                            renderer->panWorldTo(def::SectorCoords{
                                .pos = coordsTr.pos,
                                .sectorPos = coordsTr.sectorPos,
                            });
                        }
                        else
                        {
                            renderer->panWorldTo(def::SectorCoords{
                                .pos = sectorCoords,
                                .sectorPos = newPos,
                            });
                        }
                    }
                }
            }
        }
    }

    DO_PERIODIC_U_EXTNOW(lastFastCliServ,
                         intFastCliServ,
                         frametime,
                         [this]() { fastClientToServerUpdate(); });

    if (timeSyncData.cnt == 0)
    {
        DO_PERIODIC_U_EXTNOW(
            lastTSync, 2000000, frametime, [this]() { timeSync(); });
    }
    else
    {
        DO_PERIODIC_U_EXTNOW(
            lastTSync, 50000, frametime, [this]() { timeSync(); });
    }

    DO_PERIODIC_U_EXTNOW(lastReqAllComponents,
                         1000000,
                         frametime,
                         [this]()
                         { reqAllComponents(clientInfo.activeEntity); });
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
    if (renderer->getViewMode() == gfx::GameViewMode::Map)
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
                    // todo: give the server some time to init the clients
                    // otherwise auth fails. Do this properly
                    SLEEP_S(1);
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
                cmddes.value4b(realtimeZoomThr);
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
                switch (afterConnectState)
                {
                    case AfterConnectState::Menu:
                        gameState = ClientGameState::MainMenu;
                        break;
                    case AfterConnectState::Game:
                        gameState = ClientGameState::GameLoop;
                        break;
                    default:
                        LG_E("Not implemented");
                        break;
                }
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
        case prot::cmd::CLEAR_DBGCOLLAVOID:
        {
            uint32_t sectorId;
            cmddes.value4b(sectorId);
            auto sector = world.getSector(sectorId);
            if (!sector)
            {
                return;
            }
            sector->collAvoids.clear();
            break;
        }
        case prot::cmd::SEND_DATA_DBGCOLLAVOID:
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
                    ecs::EntityId id1;
                    ecs::EntityId id2;
                    vec2 intersect;
                    cmddes.object(handle);
                    cmddes.object(id1);
                    cmddes.object(id2);
                    cmddes.object(intersect);
                    sector->collAvoids.updateObject(
                        handle,
                        opool::DbgCollAvoidClient::Params{
                            .id1 = id1, .id2 = id2, .intersect = intersect});
                });
            break;
        }
        case prot::cmd::UPD_ECS_REALTIME:
            handleEcsRealtime(cmddes, dataEndPos);
            break;
        case prot::cmd::UPD_ECS_MAP:
            handleEcsMap(cmddes, dataEndPos);
            break;
        case prot::cmd::DBG_COLLAVOID_INFO_OLD:
            dbgCollAvoidBp.clear();
            while ((int)cmddes.adapter().currentReadPos()
                   <= (int)(dataEndPos)-12)
            {
                vec3 quad;
                cmddes.object(quad);
                dbgCollAvoidBp.push_back(quad);
            }
            break;
        case prot::cmd::UPD_GEN_INFO:
            handleUpdGeneral(cmddes, dataEndPos);
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

bool Model::shouldDrawRealtime(gfx::RenderEngine& renderer)
{
    return renderer.getViewMode() == gfx::GameViewMode::ThirdPerson
           || (renderer.getViewMode() == gfx::GameViewMode::Map
               && renderer.getWorldZoom() >= realtimeZoomThr);
}

void Model::drawMap(gfx::RenderEngine& renderer)
{
    long frametime = tim::nowU();
    std::vector<RealtimeDrawBounds> bounds;
    createDrawBounds(bounds, renderer.getWorldZoom() >= realtimeZoomThr);

    if (renderer.getWorldZoom() < realtimeZoomThr)
    {
        drawMapIcons(renderer, bounds);
    }
    else
    {
        drawRealtime(renderer, bounds);
    }

    // world.drawStrategicMap(renderer, viewRect, zoom);
    // if (overlayAabbTreeEnabled)
    // {
    //     drawOverlayAABBs(renderer, zoom);
    // }
}

void Model::drawThirdPerson(gfx::RenderEngine& renderer)
{
    long frametime = tim::nowU();
    std::vector<RealtimeDrawBounds> bounds;
    createDrawBounds(bounds, true);
    drawRealtime(renderer, bounds);
}


void Model::drawMapIcons(gfx::RenderEngine& renderer,
                         const vector<RealtimeDrawBounds>& drawBounds)
{
    float zoom = renderer.getWorldZoom();
    auto& reg = clientRegistry.getRegistry();
    reg.view<ecs::EntityId, TransformHist, ecs::MapIcon, ecs::FactionId>().each(
        [this, &renderer, &reg, &drawBounds, zoom](game_entity entity,
                                                   ecs::EntityId entityId,
                                                   TransformHist& tr,
                                                   ecs::MapIcon& mapIcon,
                                                   ecs::FactionId& factionId)
        {
            ClientTransform clitr;
            if (!tr.interpolate(rendertime, clitr, {.world = &world})
                && !tr.interpolate(rendertime, clitr, {.world = &world}, true))
            {
                return;
            }
            // Check if in any visible sector
            for (auto& bounds : drawBounds)
            {
                if (bounds.sectorId != clitr.sectorId)
                {
                    continue;
                }
                if (bounds.aabb.containsPoint(clitr.tr.pos))
                {
                    auto* mapIconItem = modManager->getMapIconLib().getItem(
                        gobj::MapIconHandle(mapIcon.mapIconHandle));
                    if (mapIconItem)
                    {
                        glm::vec2 worldPos = world.getWorldPosSectorOffset(
                                                 clitr.sectorId,
                                                 renderer.getSectorOffsetX(),
                                                 renderer.getSectorOffsetY())
                                             + clitr.tr.pos;
                        mod::MappedTextureHandle mTexHandle =
                            *(mod::MappedTextureHandle*)&mapIconItem->texHandle;
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
                            factionColTest(factionId.faction));
                        if (isSelected(entityId))
                        {
                            renderer.drawShapeRectangle(
                                worldPos,
                                glm::vec2((mapIconItem->size.x + 8.0f) / zoom,
                                          (mapIconItem->size.y + 8.0f) / zoom),
                                factionColTest(factionId.faction),
                                1.0f / zoom);
                        }
                    }
                }
                break;
            }
        });
}

void Model::drawRealtime(gfx::RenderEngine& renderer,
                         const vector<RealtimeDrawBounds>& bounds)
{
    long frametime = tim::nowU();
    long renderTime = frametime - timeSyncData.serverLatency - realtimeDelay;
    drawRealtimeShips(renderer, bounds);
    drawRealtimeAsteroids(renderer, bounds);
    drawRealtimeProjectiles(renderer, bounds);
    drawRealtimeBeams(renderer, bounds);
    drawRealtimeItems(renderer, bounds);
    drawRealtimeCollavoids(renderer, bounds);

    // debug
    auto sectorId = getActiveSectorId();

    for (auto quad : dbgCollAvoidBp)
    {
        glm::vec2 worldPos =
            world.getWorldPosSectorOffset(sectorId,
                                          renderer.getSectorOffsetX(),
                                          renderer.getSectorOffsetY())
            + vec2{quad.x, quad.y};
        renderer.drawShapeRectangle(worldPos,
                                    {quad.z, quad.z},
                                    0x10ffffff,
                                    1.0f / renderer.getWorldZoom(),
                                    0.0f,
                                    0);
    }

    // for (auto bound : bounds)
    // {
    //     glm::vec2 worldPos =
    //         world.getWorldPosSectorOffset(bound.sectorId,
    //                                       renderer.getSectorOffsetX(),
    //                                       renderer.getSectorOffsetY());
    //     vec2 pos = (bound.aabb.lower + bound.aabb.upper) / 2.0f;
    //     vec2 size = bound.aabb.upper - bound.aabb.lower;
    //     renderer.drawShapeRectangle(worldPos + pos,
    //                                 size,
    //                                 0xf00000ff,
    //                                 4.0f / renderer.getWorldZoom(),
    //                                 0.0f,
    //                                 0);
    // }
}

void Model::createDrawBounds(vector<RealtimeDrawBounds>& bounds, bool realtime)
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
                if (realtime && !sectorActive(sector->getId()))
                {
                    continue;
                }
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

void Model::shutdownLocalServer()
{
    prot::MsgComposer mcomp(net::SendType::TCP, nullptr);
    mcomp.startCommand(prot::cmd::SHUTDOWN, 0);
    mcomp.execute(sendQueue);
}

void Model::reset()
{
    clientRegistry.clearSession();
    dbgCollAvoidBp.clear();
    activeSectors.clear();
    loadWorldSequence.reset();
    net::CmdQueueData sendData;
    while (sendQueue.try_dequeue(sendData))
    {
    }
    frametime = 0U;
    rendertime = 0U;
    lastTSync = 0U;
    lastFastCliServ = 0U;
    lastReqAllComponents = 0U;
    lastGetAabbTree = 0U;
    aabbs.clear();
    timeSyncData.serverLatency = 0U;
    timeSyncData.serverOffset = 0U;
}

void Model::drawRealtimeShips(gfx::RenderEngine& renderer,
                              const vector<RealtimeDrawBounds>& drawBounds)
{
    auto& reg = clientRegistry.getRegistry();
    reg.view<ecs::EntityId,
             TransformHist,
             ecs::Textures,
             ecs::Hull,
             ecs::Collider>()
        .each(
            [this, &renderer, &reg, &drawBounds](game_entity entity,
                                                 ecs::EntityId entityId,
                                                 TransformHist& tr,
                                                 ecs::Textures& textures,
                                                 ecs::Hull& hull,
                                                 ecs::Collider& coll)
            {
                sphyc::ClientTransform clitr;
                if (!tr.interpolate(rendertime, clitr, {.world = &world}))
                {
                    return;
                }
                const auto& trInt = clitr.tr;
                // Check if in any visible sector
                for (auto& bounds : drawBounds)
                {
                    if (bounds.sectorId != clitr.sectorId)
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
                    const con::AABB aabb{.lower = trInt.pos - centerDistVec,
                                         .upper = trInt.pos + centerDistVec};
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
                                             clitr.sectorId,
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

                    if (renderer.getViewMode() == gfx::GameViewMode::Map
                        && isSelected(entityId))
                    {
                        const float size = centerDist * 2.3f;
                        const float zoom = renderer.getWorldZoom();
                        renderer.drawShapeRectangle(worldPos,
                                                    glm::vec2((size), (size)),
                                                    0xa000ff00,
                                                    1.0f / zoom);
                    }
                    break;
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
//             bool sectorFilter = activeSectorId ==
//             world::INVALID_SECTOR_ID
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
                                  const vector<RealtimeDrawBounds>& drawBounds)
{
    auto& reg = clientRegistry.getRegistry();
    reg.view<TransformHist,
             ecs::SectorId,
             ecs::Asteroid,
             ecs::Textures,
             ecs::Collider>()
        .each(
            [this, &renderer, &reg, &drawBounds](TransformHist& tr,
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
                    break;
                }
            });
}

void Model::drawRealtimeItems(gfx::RenderEngine& renderer,
                              const vector<RealtimeDrawBounds>& drawBounds)
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
            [&renderer, &visibleBounds, this, &bound](opool::ItemClient& item)
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
    const vector<RealtimeDrawBounds>& drawBounds)
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
            [&renderer, &visibleBounds, this, &bound](opool::ProjClient& proj)
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
                              const vector<RealtimeDrawBounds>& drawBounds)
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
            [&renderer, &visibleBounds, this, &bound](opool::BeamClient& beam)
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

void Model::drawRealtimeCollavoids(gfx::RenderEngine& renderer,
                                   const vector<RealtimeDrawBounds>& drawBounds)
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
        sector->collAvoids.foreach (
            [&renderer, &visibleBounds, this, &bound](
                opool::DbgCollAvoidClient& collAvoid)
            {
                if (visibleBounds.containsPoint(collAvoid.intersect))
                {
                    auto entt1 = clientRegistry.enttFromServerId(collAvoid.id1);
                    auto entt2 = clientRegistry.enttFromServerId(collAvoid.id2);
                    auto tr1 =
                        clientRegistry.getRegistry().try_get<TransformHist>(
                            entt1);
                    auto tr2 =
                        clientRegistry.getRegistry().try_get<TransformHist>(
                            entt2);
                    sphyc::ClientTransform ctr1, ctr2;
                    if (tr1 && tr2
                        && tr1->interpolate(rendertime, ctr1, {.world = &world})
                        && tr2->interpolate(
                            rendertime, ctr2, {.world = &world}))
                    {
                        const vec2 offs = world.getWorldPosSectorOffset(
                            bound.sectorId,
                            renderer.getSectorOffsetX(),
                            renderer.getSectorOffsetY());
                        renderer.drawLine(offs + ctr1.tr.pos,
                                          offs + collAvoid.intersect,
                                          0x502222ff,
                                          1.0 / renderer.getWorldZoom());
                        renderer.drawLine(offs + ctr2.tr.pos,
                                          offs + collAvoid.intersect,
                                          0x502222ff,
                                          1.0 / renderer.getWorldZoom());
                        renderer.drawShapeRectangle(
                            offs + collAvoid.intersect,
                            vec2(50.0f, 50.0f),
                            0x502222ff,
                            2.0 / renderer.getWorldZoom());
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

void Model::checkVersion(const net::ConnectData& connectData,
                         AfterConnectState after)
{
    reset();
    afterConnectState = after;
    this->clientInfo =
        def::ClientInfo("", connectData, 0, GenericHandle::Invalid());
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
    mcomp.ser->text1b(clientInfo.connectData.token, 16);
    mcomp.ser->value2b((uint16_t)clientInfo.connectData.udpPortCli);
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

void Model::toggleMap()
{
    if (gameState == ClientGameState::GameLoop)
    {
        renderer->clbToggleMap();
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

void Model::reqAllComponents(ecs::EntityId entityId)
{
    prot::MsgComposer mcomp(net::SendType::TCP, nullptr);
    mcomp.startCommand(prot::cmd::REQ_ALL_COMPONENTS, 0);
    mcomp.ser->object(entityId);
    mcomp.execute(sendQueue);
}

ecs::EntityId
Model::clickEntityAtWorldPos(gfx::RenderEngine& renderer,
                             const def::SectorCoords& sectorCoords)
{
    selectedEntities.clear();
    auto& reg = clientRegistry.getRegistry();
    for (const auto entity : reg.view<TransformHist,
                                      ecs::EntityId,
                                      ecs::Collider,
                                      ecs::tag::Selectable>())
    {
        auto& trHist = reg.get<TransformHist>(entity);
        auto& eid = reg.get<ecs::EntityId>(entity);
        auto& coll = reg.get<ecs::Collider>(entity);

        ClientTransform clitr;
        if (!trHist.interpolate(rendertime, clitr, {.world = &world}))
        {
            continue;
        }
        if (clitr.sectorId != world.sectorCoordsToId(sectorCoords.pos))
        {
            continue;
        }
        ecs::Transform& tr = clitr.tr;
        if (shouldDrawRealtime(renderer))
        {
            if (coll.isPointInsideWorld(sectorCoords.sectorPos,
                                        tr,
                                        std::cos(tr.rot),
                                        std::sin(tr.rot),
                                        &modManager->getColliderLib()))
            {
                clickedEntity = eid;
                selectedEntities.push_back(eid);
                return clickedEntity;
            }
        }
        else
        {
            auto mapIcon = reg.try_get<ecs::MapIcon>(entity);
            if (!mapIcon)
            {
                continue;
            }
            auto* mapIconItem = modManager->getMapIconLib().getItem(
                gobj::MapIconHandle(mapIcon->mapIconHandle));
            if (!mapIconItem)
            {
                continue;
            }
            const float boundSize =
                std::max(mapIconItem->size.x, mapIconItem->size.y)
                / renderer.getWorldZoom() * 0.5f;
            const vec2 bounds{boundSize, boundSize};
            const con::AABB pointerRect{
                .lower = sectorCoords.sectorPos - bounds,
                .upper = sectorCoords.sectorPos + bounds,
            };
            if (pointerRect.containsPoint(tr.pos))
            {
                clickedEntity = eid;
                selectedEntities.push_back(eid);
                return clickedEntity;
            }
        }
    }
    return ecs::EntityId::Invalid();
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
             TransformHist,
             ecs::EntityId,
             ecs::tag::Selectable>()
        .each(
            [this, &xMin, &xMax, &yMin, &yMax](
                ecs::SectorId& sid, TransformHist& trH, ecs::EntityId& eid)
            {
                ClientTransform clitr;
                if (!trH.interpolate(rendertime, clitr, {.world = &world}))
                {
                    return;
                }
                ecs::Transform& tr = clitr.tr;
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
    if (selectedEntities.size() > 0)
    {
        clickedEntity = ecs::EntityId::Invalid();
        if (selectedEntities.size() == 1)
        {
            clickedEntity = selectedEntities.back();
        }
    }
}

void Model::clearSelectedEntities()
{
    selectedEntities.clear();
}

void Model::clearClickedEntity()
{
    clickedEntity = ecs::EntityId::Invalid();
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

void Model::handleUpdGeneral(bitsery::Deserializer<InputAdapter>& cmddes,
                             size_t dataEndPos)
{
    uint16_t actCnt;
    cmddes.value2b(actCnt);
    activeSectors.clear();
    for (int i = 0; i < actCnt; ++i)
    {
        uint32_t actSec;
        cmddes.value4b(actSec);
        activeSectors.push_back(actSec);
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