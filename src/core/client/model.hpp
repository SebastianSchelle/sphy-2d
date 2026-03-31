#ifndef MODEL_HPP
#define MODEL_HPP

#include <std-inc.hpp>
#include <concurrentqueue.h>
#include <client-def.hpp>
#include <net-shared.hpp>
#include <exchange-sequence.hpp>
#include <world.hpp>

using moodycamel::ConcurrentQueue;

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
    void parseCommand(std::vector<uint8_t> data);
    void modelLoop(float dt);
    ClientGameState getGameState() const { return gameState; }

    void startLoadingMods();
    void startModel();
    void drawDebug(gfx::RenderEngine& renderer, float zoom);
    void sendCmdToServer(const std::string& command);

    ConcurrentQueue<net::CmdQueueData> sendQueue;
    ConcurrentQueue<net::CmdQueueData> receiveQueue;

  private:
    void modelLoopMenu(float dt);
    void modelLoopGame(float dt);
    void timeSync();
    net::TimeSync timeSyncData;
    ClientGameState gameState = ClientGameState::Init;
    net::ExchangeSequence loadWorldSequence;
    world::World world;
    ui::UserInterface* userInterface;
};

}  // namespace sphyc

#endif