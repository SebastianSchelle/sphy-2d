#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "ecs.hpp"
#include <boost/asio.hpp>
#include <boost/container/vector.hpp>
#include <concurrentqueue.h>
#include <config-manager/config-manager.hpp>
#include <item-lib.hpp>
#include <client-def.hpp>
#include <net-shared.hpp>
#include <world.hpp>
#include <atomic>
#include <cmd-options.hpp>
#include <mod-manager.hpp>
#include <lua-interpreter.hpp>
#include <asset-factory.hpp>

using moodycamel::ConcurrentQueue;

namespace ecs
{
  struct PtrHandle;
}

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
    void stop();  // request shutdown, save game, join engine thread
    void registerClient(const std::string &uuid, const std::string &name);
    void saveGame();
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
    void update(float dt);

    const sphy::CmdLinOptionsServer& options;
    std::atomic<bool> stopRequested{false};
    std::thread engineThread;
    con::ItemLib<net::ClientInfo> clientLib;
    std::vector<net::ClientInfoHandle> activeClientHandles;
    mod::ModManager modManager;
    mod::LuaInterpreter luaInterpreter;

    EngineState state;
    world::World world;
    cfg::ConfigManager saveConfig;
    std::string saveFolder;
    ecs::Ecs ecs;
    ecs::AssetFactory assetFactory;
    vector<ecs::EntityId> globalEntityIds;
    vector<entt::entity> globalEntities;
    std::shared_ptr<ecs::PtrHandle> ptrHandle;
};

}  // namespace sphys

#endif