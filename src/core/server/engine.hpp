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
#include <world.hpp>
#include <cmd-options.hpp>
#include <mod-manager.hpp>
#include <lua-interpreter.hpp>

using moodycamel::ConcurrentQueue;

namespace sphys
{

enum class EngineState
{
    Init,
    LoadMods,
    LoadWorld,
    CreateWorld,
    Running,
    Paused,
    Stopped,
    Error,
};

class Engine
{
  public:
    Engine(const sphy::CmdLinOptionsServer& options);
    ~Engine();
    void start();
    void registerClient(const std::string &uuid, const std::string &name);

    ConcurrentQueue<net::CmdQueueData> sendQueue;
    ConcurrentQueue<net::CmdQueueData> receiveQueue;

  private:
    void engineLoop();
    void startFromFolder();
    void parseCommand(const net::CmdQueueData& cmdData);
    bool loadFromFolder();
    bool createFromConfig();
    bool loadWorld();
    bool saveWorld();
    bool createWorld();
    bool loadMods();

    const sphy::CmdLinOptionsServer& options;
    std::thread engineThread;
    con::ItemLib<net::ClientInfo> clientLib;
    std::vector<net::ClientInfoHandle> activeClientHandles;
    mod::ModManager modManager;
    mod::LuaInterpreter luaInterpreter;

    EngineState state;
    world::World world;
    cfg::ConfigManager saveConfig;
    std::string saveFolder;
};

}  // namespace sphys

#endif