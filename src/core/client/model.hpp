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
    void start();
    void stop();
    void wait();  // Wait for model thread to finish
    void parseCommand(std::vector<uint8_t> data);

    ConcurrentQueue<net::CmdQueueData> sendQueue;
    ConcurrentQueue<net::CmdQueueData> receiveQueue;

  private:
    void modelLoop();
    void modelLoopMenu();
    void modelLoopGame();
    void timeSync();
    std::thread modelThread;
    std::atomic<bool> running{false};
    net::TimeSync timeSyncData;
    ConnectionState connectionState = ConnectionState::DISCONNECTED;
};

}  // namespace sphyc

#endif