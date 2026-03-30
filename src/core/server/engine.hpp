#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "ecs.hpp"
#include <asset-factory.hpp>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/container/vector.hpp>
#include <client-def.hpp>
#include <cmd-options.hpp>
#include <concurrentqueue.h>
#include <config-manager/config-manager.hpp>
#include <item-lib.hpp>
#include <lua-interpreter.hpp>
#include <mod-manager.hpp>
#include <net-shared.hpp>
#include <world.hpp>
#include <rerun.hpp>

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
    void registerClient(const std::string& uuid, const std::string& name, net::ClientFlags flags);
    void saveGame();
    bool stopped() const
    {
        return stopRequested;
    }
    ecs::EntityId spawnEntityFromAsset(const std::string& assetId);
    ecs::EntityId spawnEntityFromAsset(const std::string& assetId,
                                       uint32_t sectorId,
                                       const ecs::Transform& transform);
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
    void postWorldSetup();

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
    ecs::AssetFactory assetFactory;
    vector<ecs::EntityId> globalEntityIds;
    vector<entt::entity> globalEntities;
    std::shared_ptr<ecs::PtrHandle> ptrHandle;
    rerun::RecordingStream rerunStream;

  public:
    ecs::Ecs ecs;
};

}  // namespace sphys

#endif