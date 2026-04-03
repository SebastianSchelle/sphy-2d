#ifndef MODEL_HPP
#define MODEL_HPP

#include <std-inc.hpp>
#include <client-def.hpp>
#include <net-shared.hpp>
#include <exchange-sequence.hpp>
#include <world.hpp>
#include <ecs.hpp>

namespace ui
{
    class UserInterface;
}

namespace sphyc
{

class Model
{
  public:
    Model(ui::UserInterface* userInterface);
    ~Model();
    void parseCommandData(const net::CmdQueueData& cmdData);
    void parseCommand(bitsery::Deserializer<InputAdapter>& cmddes,
                      uint16_t cmd,
                      uint8_t flags,
                      uint16_t len);
    void modelLoop(float dt);
    ClientGameState getGameState() const { return gameState; }

    void startLoadingMods();
    void startModel();
    void drawDebug(gfx::RenderEngine& renderer, float zoom);
    void sendCmdToServer(const std::string& command);
    const net::TimeSync& getTimeSyncData() const { return timeSyncData; }
    const net::ModelClientInfo& getClientInfo() const { return clientInfo; }
    void checkVersion(const net::ModelClientInfo& clientInfo);
    void disconnectFromServer();
    ecs::AssetFactory* getAssetFactory() { return &assetFactory; }
    ConcurrentQueue<net::CmdQueueData> sendQueue;
    ConcurrentQueue<net::CmdQueueData> receiveQueue;

  private:
    void modelLoopMenu(float dt);
    void modelLoopGame(float dt);
    void timeSync();
    void authenticate();
    void handleSlowDump(bitsery::Deserializer<InputAdapter>& cmddes, uint16_t numBytes);
    net::TimeSync timeSyncData;
    ClientGameState gameState = ClientGameState::Init;
    net::ExchangeSequence loadWorldSequence;
    world::World world;
    ui::UserInterface* userInterface;
    ecs::AssetFactory assetFactory;
    tim::Timepoint lastTSync;
    net::ModelClientInfo clientInfo;
    ecs::EcsClient ecs;
};

}  // namespace sphyc

#endif