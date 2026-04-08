#ifndef MODEL_HPP
#define MODEL_HPP

#include <client-def.hpp>
#include <ecs.hpp>
#include <exchange-sequence.hpp>
#include <net-shared.hpp>
#include <std-inc.hpp>
#include <world.hpp>

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
          std::function<void(void)> afterLoadWorldClb);
    ~Model();
    void parseCommandData(const net::CmdQueueData& cmdData);
    void parseCommand(bitsery::Deserializer<InputAdapter>& cmddes,
                      uint16_t cmd,
                      uint8_t flags,
                      uint16_t len);
    void modelLoop(float dt);
    ClientGameState getGameState() const
    {
        return gameState;
    }

    void startLoadingMods();
    void startModel();
    void drawDebug(gfx::RenderEngine& renderer, float zoom);
    void sendCmdToServer(const std::string& command);
    const net::TimeSync& getTimeSyncData() const
    {
        return timeSyncData;
    }
    const net::ModelClientInfo& getClientInfo() const
    {
        return clientInfo;
    }
    void checkVersion(const net::ModelClientInfo& clientInfo);
    void disconnectFromServer();
    ecs::AssetFactory* getAssetFactory()
    {
        return &assetFactory;
    }
    ConcurrentQueue<net::CmdQueueData> sendQueue;
    ConcurrentQueue<net::CmdQueueData> receiveQueue;
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
        return selectedEntity;
    }
    ecs::EcsClient* getEcs()
    {
        return &ecs;
    }
    world::World& getWorld()
    {
        return world;
    }
    void selectEntitiesInsideRect(const def::SectorCoords& start,
                                  const def::SectorCoords& end);

  private:
    void modelLoopMenu(float dt);
    void modelLoopGame(float dt);
    void timeSync();
    void authenticate();
    void handleSlowDump(bitsery::Deserializer<InputAdapter>& cmddes,
                        uint16_t posNextCmdOrEof);
    void reqAllComponents(ecs::EntityId entity);
    void handleReqAllComponentsResp(bitsery::Deserializer<InputAdapter>& cmddes,
                                    uint16_t posNextCmdOrEof);

    cfg::ConfigManager& config;
    net::TimeSync timeSyncData;
    ClientGameState gameState = ClientGameState::Init;
    net::ExchangeSequence loadWorldSequence;
    world::World world;
    ui::UserInterface* userInterface;
    ecs::AssetFactory assetFactory;
    tim::Timepoint lastTSync;
    net::ModelClientInfo clientInfo;
    ecs::EcsClient ecs;
    ecs::EntityId selectedEntity;

    std::function<void(void)> afterLoadWorldClb;
};

}  // namespace sphyc

#endif