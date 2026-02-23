#ifndef MODEL_HPP
#define MODEL_HPP

#include <std-inc.hpp>
#include <concurrentqueue.h>
#include <client-def.hpp>
#include <net-shared.hpp>
#include <exchange-sequence.hpp>
#include <world.hpp>

using moodycamel::ConcurrentQueue;

namespace sphyc
{

class Model
{
  public:
    Model();
    ~Model();
    void parseCommand(std::vector<uint8_t> data);
    void modelLoop(float dt);
    ClientGameState getGameState() const { return gameState; }

    void startLoadingMods();
    void startModel();
    void drawDebug(gfx::RenderEngine& renderer, float zoom);

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
};

}  // namespace sphyc

#endif