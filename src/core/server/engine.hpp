#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <boost/asio.hpp>
#include <boost/container/vector.hpp>
#include <concurrentqueue.h>
#include <config-manager/config-manager.hpp>
#include <sphy-2d.hpp>

using moodycamel::ConcurrentQueue;
namespace asio = boost::asio;

namespace sphys
{

enum class SendType
{
    UDP,
    TCP
};

typedef struct
{
    SendType type;
    asio::ip::address address;
    con::vector<uint8_t> data;
} SendRequest;

class Engine
{
  public:
    Engine();
    ~Engine();
    void start();
    ConcurrentQueue<SendRequest> sendQueue;

  private:
    void engineLoop();

    cfg::ConfigManager configManager;
    std::thread engineThread;
};

}  // namespace sphys

#endif