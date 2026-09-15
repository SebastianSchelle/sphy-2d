#ifndef MODEL_HPP
#define MODEL_HPP

#include "comp-ident.hpp"
#include "glm/common.hpp"
#include "render-engine.hpp"
#include "sector.hpp"
#include "world-def.hpp"
#include <RmlUi/Core/DataModelHandle.h>
#include <algorithm>
#include <asset-factory.hpp>
#include <client-def.hpp>
#include <client-pool-obj.hpp>
#include <client-registry.hpp>
#include <control-def.hpp>
#include <exchange-sequence.hpp>
#include <net-shared.hpp>
#include <obj-pool-client.hpp>
#include <std-inc.hpp>
#include <world.hpp>

namespace ecs
{
struct Textures;
struct Hull;
struct Station;
}  // namespace ecs

namespace mod
{
class ModManager;
}

namespace ui
{
class UserInterface;
}

namespace sphyc
{

struct DragSelectionHelper
{
    int32_t secXMin;
    int32_t secXMax;
    int32_t secYMin;
    int32_t secYMax;
    float posXMin;
    float posXMax;
    float posYMin;
    float posYMax;
};

struct ConnectingData
{
    string status;
    string info;
};

struct RealtimeDrawBounds
{
    uint32_t sectorId;
    con::AABB aabb;
};

struct ClientTransform
{
    ecs::Transform tr;
    uint32_t sectorId;

    struct ExtraParam
    {
        world::World* world;
    };
    ClientTransform mix(const ClientTransform& other,
                        float alpha,
                        const ExtraParam& extra) const
    {
        const float mixRot =
            tr.rot + smath::angleError(other.tr.rot, tr.rot) * alpha;
        // uint32_t newSector = other.sectorId;
        if (sectorId != other.sectorId && extra.world)
        {
            // make it more sofisticated: set correct sectorId to not extend
            // sector bounds, maybe this fucks with drawing and world pos
            const vec2 prevPosTr =
                extra.world->translateCoords(tr.pos, sectorId, other.sectorId);
            const vec2 moveVec = other.tr.pos - prevPosTr;
            const vec2 interPos = tr.pos + alpha * moveVec;
            const def::SectorPos prevSectorXY =
                extra.world->idToSectorCoords(sectorId);
            const def::SectorCoords coordsTr = extra.world->translateOOBCoords(
                {.pos = prevSectorXY, .sectorPos = interPos});
            return {.tr = {.pos = coordsTr.sectorPos, .rot = mixRot},
                    .sectorId = extra.world->sectorCoordsToId(coordsTr.pos)};
        }
        else
        {
            const vec2 mixPos = glm::mix(tr.pos, other.tr.pos, alpha);
            return {.tr = {.pos = mixPos, .rot = mixRot},
                    .sectorId = other.sectorId};
        }
    }
};

typedef InterpolData<ClientTransform> TransformHist;

class Model
{
  public:
    Model(ui::UserInterface* userInterface,
          cfg::ConfigManager& config,
          mod::ModManager* modManager,
          gfx::RenderEngine* renderer,
          std::function<void(void)> afterLoadWorldClb);
    ~Model();
    void modelLoop(float dt);

    void startLoadingMods();
    void startModel();
    void
    drawMap(gfx::RenderEngine& renderer, const glm::vec4& viewRect, float zoom);
    void drawMap(gfx::RenderEngine& renderer);
    void drawThirdPerson(gfx::RenderEngine& renderer);
    void setOverlayEnabled(const std::string& overlay, bool enabled);
    bool isAabbTreeOverlayEnabled() const;
    void sendCmdToServer(const std::string& command);
    void checkVersion(const net::ModelClientInfo& clientInfo);
    void prepareForConnect();
    void disconnectFromServer();
    ConcurrentQueue<net::CmdQueueData> sendQueue;
    ConcurrentQueue<net::CmdQueueData> receiveQueue;

    ecs::EntityId clickEntityAtWorldPos(gfx::RenderEngine& renderer,
                                        const def::SectorCoords& sectorCoords);
    void selectEntitiesInsideRect(const def::SectorCoords& start,
                                  const def::SectorCoords& end);
    void clearSelectedEntities();
    void clearClickedEntity();
    void selectedEntitiesMoveCmd(def::SectorCoords& sectorCoords, bool queue);
    void gotoModdingTools();
    void gotoAtlasDebug();
    void gotoMenu();
    void setClientViewRect(const def::ClientViewRect& cvr)
    {
        clientInfo.clientViewRect = cvr;
    }
    void toggleMap();
    void centerViewOnPlayer();
    void setupDataModelConnecting();
    void setCurrentTime(gfx::RenderEngine& renderer, long frametime);

    def::ThirdPersonControl& getThirdPersonControl()
    {
        return thirdPersonControl;
    }

    const std::vector<ecs::EntityId>& getSelectedEntities() const
    {
        return selectedEntities;
    }
    ecs::EntityId getClickedEntity() const
    {
        return clickedEntity;
    }
    const def::WorldShape& getWorldShape() const
    {
        return world.getWorldShape();
    }
    Registry& getRegistry()
    {
        return clientRegistry.getRegistry();
    }
    ecs::EntityId getSelectedEntity() const
    {
        return clientInfo.activeEntity;
    }
    ecs::ClientRegistry* getClientRegistry()
    {
        return &clientRegistry;
    }
    world::World& getWorld()
    {
        return world;
    }
    ecs::AssetFactory* getAssetFactory()
    {
        return &assetFactory;
    }
    const net::TimeSync& getTimeSyncData() const
    {
        return timeSyncData;
    }
    const def::ClientInfo& getClientInfo() const
    {
        return clientInfo;
    }
    ClientGameState getGameState() const
    {
        return gameState;
    }
    bool sectorActive(uint32_t id)
    {
        return std::find(activeSectors.begin(), activeSectors.end(), id)
               != activeSectors.end();
    }
    bool isSelected(ecs::EntityId id)
    {
        return std::find(selectedEntities.begin(), selectedEntities.end(), id)
               != selectedEntities.end();
    }

  private:
    void parseCommandData(const net::CmdQueueData& cmdData);
    void parseCommand(bitsery::Deserializer<InputAdapter>& cmddes,
                      net::SendType sendType,
                      uint16_t cmd,
                      uint8_t flags,
                      size_t dataEndPos);
    void modelLoopMenu(float dt);
    void modelLoopGame(float dt);
    void timeSync();
    void authenticate();
    void handleSlowDump(bitsery::Deserializer<InputAdapter>& cmddes,
                        size_t dataEndPos);
    void handleActiveSectorDump(bitsery::Deserializer<InputAdapter>& cmddes,
                                size_t dataEndPos);
    void reqAllComponents(ecs::EntityId entity);
    void handleReqAllComponentsResp(bitsery::Deserializer<InputAdapter>& cmddes,
                                    size_t dataEndPos);
    void handleDestroyEntity(bitsery::Deserializer<InputAdapter>& cmddes,
                             size_t dataEndPos);
    void handleEcsRealtime(bitsery::Deserializer<InputAdapter>& cmddes,
                           size_t dataEndPos);
    void handleEcsMap(bitsery::Deserializer<InputAdapter>& cmddes,
                      size_t dataEndPos);
    void handleUpdGeneral(bitsery::Deserializer<InputAdapter>& cmddes,
                          size_t dataEndPos);
    void handleSendOpool(
        bitsery::Deserializer<InputAdapter>& cmddes,
        size_t dataEndPos,
        size_t juckSize,
        std::function<void(world::Sector* sector,
                           bitsery::Deserializer<InputAdapter>& cmddes,
                           long frametime)> clb);
    void notifyReady();
    void handleGetAabbTreeResp(bitsery::Deserializer<InputAdapter>& cmddes,
                               size_t dataEndPos);
    void handleActiveEntitySwitched(bitsery::Deserializer<InputAdapter>& cmddes,
                                    size_t dataEndPos);
    void drawOverlayAABBs(gfx::RenderEngine& renderer, float zoom);
    bool shouldDrawRealtime(gfx::RenderEngine& renderer);

    // Map drawing
    void drawMapIcons(gfx::RenderEngine& renderer,
                      const vector<RealtimeDrawBounds>& drawBounds);
    // void drawSelected(gfx::RenderEngine& renderer,
    //                   const vector<RealtimeDrawBounds>& drawBounds,
    //                   bool isRealtime);

    // Realtime drawing
    void drawRealtime(gfx::RenderEngine& renderer,
                      const vector<RealtimeDrawBounds>& bounds);
    void createDrawBounds(vector<RealtimeDrawBounds>& bounds, bool realtime);
    void drawRealtimeShips(gfx::RenderEngine& renderer,
                           const vector<RealtimeDrawBounds>& drawBounds);
    // void drawRealtimeStations(gfx::RenderEngine& renderer,
    //    const vector<RealtimeDrawBounds>& drawBounds);
    void drawRealtimeItems(gfx::RenderEngine& renderer,
                           const vector<RealtimeDrawBounds>& drawBounds);
    void drawRealtimeProjectiles(gfx::RenderEngine& renderer,
                                 const vector<RealtimeDrawBounds>& drawBounds);
    void drawRealtimeBeams(gfx::RenderEngine& renderer,
                           const vector<RealtimeDrawBounds>& drawBounds);
    void drawRealtimeAsteroids(gfx::RenderEngine& renderer,
                               const vector<RealtimeDrawBounds>& drawBounds);
    void drawRealtimeCollavoids(gfx::RenderEngine& renderer,
                                const vector<RealtimeDrawBounds>& drawBounds);
    void drawTexture(gfx::RenderEngine& renderer,
                     const GenericHandle texture,
                     float rot,
                     const vec2& size,
                     const int8_t parentZ,
                     const glm::vec2& worldPos,
                     bool useTexSize = false);
    void drawTextures(gfx::RenderEngine& renderer,
                      const ecs::Textures& textures,
                      float rot,
                      const int8_t zIndex,
                      const glm::vec2& worldPos);
    void drawTextures(gfx::RenderEngine& renderer,
                      gobj::TexturesHandle texHandle,
                      float rot,
                      const int8_t parentZ,
                      const glm::vec2& worldPos);
    void drawModuleTextures(gfx::RenderEngine& renderer,
                            const ecs::Transform& parentTransform,
                            const int8_t parentZ,
                            ecs::Hull& hull,
                            const glm::vec2& worldPos);
    void drawStationTextures(gfx::RenderEngine& renderer,
                             const ecs::Transform& parentTransform,
                             ecs::Station& station,
                             const glm::vec2& sectorOffset);
    void registerConnectSequence();
    uint32_t getActiveSectorId();
    game_entity getActiveEntity();
    void fastClientToServerUpdate();

    cfg::ConfigManager& config;
    net::TimeSync timeSyncData;
    ClientGameState gameState = ClientGameState::Init;
    net::ExchangeSequence loadWorldSequence;
    world::World world;
    ui::UserInterface* userInterface;
    mod::ModManager* modManager;
    gfx::RenderEngine* renderer;
    Rml::DataModelHandle rmlModelConnecting;
    ConnectingData connectingData;
    ecs::AssetFactory assetFactory;
    def::ClientInfo clientInfo;
    ecs::ClientRegistry clientRegistry;

    std::function<void(void)> afterLoadWorldClb;
    std::vector<ecs::EntityId>
        selectedEntities;  // todo: sorted with proper binary search enabled??
    ecs::EntityId clickedEntity;

    uint32_t aabbSector;
    std::vector<con::AABB> aabbs;
    bool overlayAabbTreeEnabled = false;

    long lastGetAabbTree;
    long lastReqAllComponents;
    long lastFastCliServ;
    long lastTSync;

    def::ThirdPersonControl thirdPersonControl;
    uint16_t intFastCliServ;
    long realtimeDelay;
    float realtimeZoomThr;
    long mapDelay;

    vector<vec3> dbgCollAvoidBp;
    vector<uint32_t> activeSectors;
    long frametime;
    long rendertime;
};

}  // namespace sphyc

#endif