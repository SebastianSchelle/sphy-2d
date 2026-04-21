#ifndef MODEL_HPP
#define MODEL_HPP

#include <client-def.hpp>
#include <ecs.hpp>
#include <exchange-sequence.hpp>
#include <net-shared.hpp>
#include <std-inc.hpp>
#include <world.hpp>

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
    void drawDebug(gfx::RenderEngine& renderer, float zoom);
    void drawTacticalMap(gfx::RenderEngine& renderer,
                         const glm::vec4& viewRect,
                         float zoom);
    void drawStrategicMap(gfx::RenderEngine& renderer,
                          const glm::vec4& viewRect,
                          float zoom);
    void drawThirdPerson(gfx::RenderEngine& renderer,
                         const glm::vec4& viewRect,
                         float zoom);
    void setOverlayEnabled(const std::string& overlay, bool enabled);
    bool isAabbTreeOverlayEnabled() const;
    void sendCmdToServer(const std::string& command);
    void checkVersion(const net::ModelClientInfo& clientInfo);
    void disconnectFromServer();
    ConcurrentQueue<net::CmdQueueData> sendQueue;
    ConcurrentQueue<net::CmdQueueData> receiveQueue;

    void selectEntitiesInsideRect(const def::SectorCoords& start,
                                  const def::SectorCoords& end);
    void clearSelectedEntities();
    void selectedEntitiesMoveCmd(def::SectorCoords& sectorCoords);

    const std::vector<ecs::EntityId>& getSelectedEntities() const
    {
        return selectedEntities;
    }
    const def::WorldShape& getWorldShape() const
    {
        return world.getWorldShape();
    }
    entt::registry& getRegistry()
    {
        return ecs.getRegistry();
    }
    ecs::EntityId getSelectedEntity() const
    {
        return clientInfo.getActiveEntity();
    }
    ecs::EcsClient* getEcs()
    {
        return &ecs;
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
                      uint16_t len);
    void modelLoopMenu(float dt);
    void modelLoopGame(float dt);
    void timeSync();
    void authenticate();
    void handleSlowDump(bitsery::Deserializer<InputAdapter>& cmddes,
                        uint16_t posNextCmdOrEof);
    void reqAllComponents(ecs::EntityId entity);
    void handleReqAllComponentsResp(bitsery::Deserializer<InputAdapter>& cmddes,
                                    uint16_t posNextCmdOrEof);
    void notifyReady();
    void handleGetAabbTreeResp(bitsery::Deserializer<InputAdapter>& cmddes,
                               uint16_t posNextCmdOrEof);
    void handleActiveEntitySwitched(bitsery::Deserializer<InputAdapter>& cmddes,
                                    uint16_t posNextCmdOrEof);
    void drawOverlayAABBs(gfx::RenderEngine& renderer, float zoom);
    void drawTextures(gfx::RenderEngine& renderer,
                      const glm::vec4& viewRect,
                      float zoom);

    cfg::ConfigManager& config;
    net::TimeSync timeSyncData;
    ClientGameState gameState = ClientGameState::Init;
    net::ExchangeSequence loadWorldSequence;
    world::World world;
    ui::UserInterface* userInterface;
    mod::ModManager* modManager;
    gfx::RenderEngine* renderer;
    ecs::AssetFactory assetFactory;
    tim::Timepoint lastTSync;
    def::ClientInfo clientInfo;
    ecs::EcsClient ecs;

    std::function<void(void)> afterLoadWorldClb;
    std::vector<ecs::EntityId> selectedEntities;

    uint32_t aabbSector;
    std::vector<con::AABB> aabbs;
    bool overlayAabbTreeEnabled = false;

    long lastGetAabbTree;
};

}  // namespace sphyc

#endif