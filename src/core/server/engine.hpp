#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <boost/asio.hpp>
#include <boost/container/vector.hpp>
#include <concurrentqueue.h>
#include <config-manager/config-manager.hpp>
#include <sphy-2d.hpp>
#include <item-lib.hpp>
#include <client-def.hpp>
#include <net-shared.hpp>

using moodycamel::ConcurrentQueue;

namespace sphys
{

class Engine
{
  public:
    Engine();
    ~Engine();
    void start();
    void registerClient(const std::string &uuid, const std::string &name);
    ConcurrentQueue<net::CmdQueueData> sendQueue;
    ConcurrentQueue<net::CmdQueueData> receiveQueue;

  private:
    void engineLoop();
    void parseCommand(const net::CmdQueueData& cmdData);

    std::thread engineThread;
    con::ItemLib<net::ClientInfo> clientLib;
    std::vector<int> activeClientIdxs;
};

}  // namespace sphys

#endif