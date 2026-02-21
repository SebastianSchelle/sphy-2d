#ifndef MODEL_HPP
#define MODEL_HPP

#include <atomic>
#include <std-inc.hpp>
#include <concurrentqueue.h>
#include <client-def.hpp>
#include <net-shared.hpp>

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

    ConcurrentQueue<net::CmdQueueData> sendQueue;
    ConcurrentQueue<net::CmdQueueData> receiveQueue;

  private:
    void modelLoopMenu(float dt);
    void modelLoopGame(float dt);
    void timeSync();
    net::TimeSync timeSyncData;
    GameState connectionState = GameState::DISCONNECTED;
};

}  // namespace sphyc

#endif