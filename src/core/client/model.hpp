#ifndef MODEL_HPP
#define MODEL_HPP

#include <std-inc.hpp>
#include <concurrentqueue.h>

using moodycamel::ConcurrentQueue;

namespace sphyc
{

class Model
{
  public:
    Model();
    ~Model();
    void start();
    void parseCommand(std::vector<uint8_t> data);

    ConcurrentQueue<CmdQueueData> sendQueue;
    ConcurrentQueue<CmdQueueData> receiveQueue;

  private:
    void modelLoop();
    std::thread modelThread;
};

}  // namespace sphyc

#endif