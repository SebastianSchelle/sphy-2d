#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <boost/asio.hpp>
#include <boost/container/vector.hpp>
#include <concurrentqueue.h>
#include <config-manager/config-manager.hpp>
#include <sphy-2d.hpp>

using moodycamel::ConcurrentQueue;

namespace sphys
{

class Engine
{
  public:
    Engine();
    ~Engine();
    void start();
    ConcurrentQueue<CmdQueueData> sendQueue;
    ConcurrentQueue<CmdQueueData> receiveQueue;

  private:
    void engineLoop();
    void parseCommand(std::vector<uint8_t> data);

    std::thread engineThread;
};

}  // namespace sphys

#endif