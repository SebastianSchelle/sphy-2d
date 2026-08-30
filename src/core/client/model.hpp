#ifndef MODEL_HPP
#define MODEL_HPP

#include "glm/common.hpp"
#include "logging.hpp"
#include "sector.hpp"
#include <RmlUi/Core/DataModelHandle.h>
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

#define INTERPOL_DEPTH 5

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

template <class T> class InterpolData
{
  public:
    void addSample(const T& sample, long ts)
    {
        newest = next(newest);
        history[newest] = sample;
        timestamps[newest] = ts;
    }
    T interpolate(long time) const
    {
        if (time > timestamps[newest])
        {
            LG_W("interpolation failed: time > newest timestamp");
            return history[newest];
        }
        uint8_t p = newest;
        uint8_t n = newest;
        uint8_t i = 0;
        while (time < timestamps[p])
        {
            if (i++ == INTERPOL_DEPTH - 1)
            {
                LG_W("Nothing found");
                return latest();
            }
            n = p;
            p = previous(p);
        }
        float alpha = (float)(time - timestamps[p]) / (float)(timestamps[n] - timestamps[p]);
        return history[p].mix(history[n], alpha);
    }
    T latest() const
    {
        return history[newest];
    }

  private:
    uint8_t next(uint8_t i) const
    {
        return (i + 1) % INTERPOL_DEPTH;
    }

    uint8_t previous(uint8_t i) const
    {
        return (i + INTERPOL_DEPTH - 1) % INTERPOL_DEPTH;
    }
    long timestamps[INTERPOL_DEPTH] = {0};
    T history[INTERPOL_DEPTH];
    uint8_t newest = 0;
};

struct ClientTransform
{
    ecs::Transform tr;
    uint32_t sectorId;

    ClientTransform mix(const ClientTransform& other, float alpha) const
    {
        return {.tr = {.pos = glm::mix(tr.pos, other.tr.pos, alpha),
                       .rot = other.tr.rot},
                .sectorId = other.sectorId};
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
    void modelLoop(float dt, long frametime);

    void startLoadingMods();
    void startModel();
    void drawDebug(gfx::RenderEngine& renderer, float zoom);
    void drawTacticalMap(gfx::RenderEngine& renderer,
                         const glm::vec4& viewRect,
                         float zoom);
    void drawStrategicMap(gfx::RenderEngine& renderer,
                          const glm::vec4& viewRect,
                          float zoom);
    void drawRealtime(gfx::RenderEngine& renderer);
    void setOverlayEnabled(const std::string& overlay, bool enabled);
    bool isAabbTreeOverlayEnabled() const;
    void sendCmdToServer(const std::string& command);
    void checkVersion(const net::ModelClientInfo& clientInfo);
    void prepareForConnect();
    void disconnectFromServer();
    ConcurrentQueue<net::CmdQueueData> sendQueue;
    ConcurrentQueue<net::CmdQueueData> receiveQueue;

    ecs::EntityId selectEntityAtWorldPos(const def::SectorCoords& sectorCoords);
    ecs::EntityId
    selectEntityAtWorldPosFast(const def::SectorCoords& sectorCoords,
                               float dist2);
    void selectEntitiesInsideRect(const def::SectorCoords& start,
                                  const def::SectorCoords& end);
    void clearSelectedEntities();
    void selectedEntitiesMoveCmd(def::SectorCoords& sectorCoords, bool queue);
    void gotoModdingTools();
    void gotoAtlasDebug();
    void gotoMenu();
    void setClientViewRect(const def::ClientViewRect& cvr)
    {
        clientInfo.clientViewRect = cvr;
    }
    void toggleTacticalView();
    void toggleStrategicView();
    void centerViewOnPlayer();
    void setupDataModelConnecting();

    def::ThirdPersonControl& getThirdPersonControl()
    {
        return thirdPersonControl;
    }

    const std::vector<ecs::EntityId>& getSelectedEntities() const
    {
        return selectedEntities;
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

  private:
    void parseCommandData(const net::CmdQueueData& cmdData);
    void parseCommand(bitsery::Deserializer<InputAdapter>& cmddes,
                      net::SendType sendType,
                      uint16_t cmd,
                      uint8_t flags,
                      size_t dataEndPos);
    void modelLoopMenu(float dt);
    void modelLoopGame(float dt, long frametime);
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
    void handleSendOpool(
        bitsery::Deserializer<InputAdapter>& cmddes,
        size_t dataEndPos,
        size_t juckSize,
        std::function<void(world::Sector* sector,
                           bitsery::Deserializer<InputAdapter>& cmddes)> clb);
    void notifyReady();
    void handleGetAabbTreeResp(bitsery::Deserializer<InputAdapter>& cmddes,
                               size_t dataEndPos);
    void handleActiveEntitySwitched(bitsery::Deserializer<InputAdapter>& cmddes,
                                    size_t dataEndPos);
    void drawOverlayAABBs(gfx::RenderEngine& renderer, float zoom);

    // Realtime drawing
    void createRealtimeDrawBounds(vector<RealtimeDrawBounds>& bounds);
    void drawRealtimeShips(gfx::RenderEngine& renderer,
                           const vector<RealtimeDrawBounds>& drawBounds,
                           long frametime);
    // void drawRealtimeStations(gfx::RenderEngine& renderer);
    void drawRealtimeItems(gfx::RenderEngine& renderer,
                           const vector<RealtimeDrawBounds>& drawBounds,
                           long frametime);
    void drawRealtimeProjectiles(gfx::RenderEngine& renderer,
                                 const vector<RealtimeDrawBounds>& drawBounds,
                                 long frametime);
    void drawRealtimeBeams(gfx::RenderEngine& renderer,
                           const vector<RealtimeDrawBounds>& drawBounds,
                           long frametime);
    // void drawRealtimeAsteroids(gfx::RenderEngine& renderer);

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
    tim::Timepoint lastTSync;
    def::ClientInfo clientInfo;
    ecs::ClientRegistry clientRegistry;

    std::function<void(void)> afterLoadWorldClb;
    std::vector<ecs::EntityId> selectedEntities;

    uint32_t aabbSector;
    std::vector<con::AABB> aabbs;
    bool overlayAabbTreeEnabled = false;

    long lastGetAabbTree;
    def::ThirdPersonControl thirdPersonControl;
    uint16_t intFastCliServ;
};

}  // namespace sphyc

#endif