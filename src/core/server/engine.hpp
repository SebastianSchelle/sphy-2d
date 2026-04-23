#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "ecs.hpp"
#include <asset-factory.hpp>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/container/vector.hpp>
#include <client-def.hpp>
#include <cmd-options.hpp>
#include <command-node.hpp>
#include <config-manager/config-manager.hpp>
#include <functional>
#include <item-lib.hpp>
#include <lua-interpreter.hpp>
#include <mod-manager.hpp>
#include <net-shared.hpp>
#include <ptr-handle.hpp>
#include <rerun.hpp>
#include <string>
#include <work-distributor.hpp>
#include <world.hpp>

namespace ecs
{
struct PtrHandle;
}

namespace sphys
{

typedef std::function<void(const net::ClientInfo* clientInfo,
                           ecs::PtrHandle* ptrHandle)>
    SlowDumpFunction;
struct SlowDumpEntry
{
    string componentName;
    SlowDumpFunction function;
};

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
    Engine(const sphy::CmdLinOptionsServer& options,
           cfg::ConfigManager& config);
    ~Engine();
    void start();
    void stop();  // request shutdown, save game, join engine thread
    def::ClientInfoHandle registerClient(const def::ClientInfo& clientInfo);
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
    template <class T> void registerSlowDumpComponent();

  private:
    void engineLoop();
    void startFromFolder();
    void parseCommandData(const net::CmdQueueData& cmdData);
    void parseCommand(bitsery::Deserializer<InputAdapter>& cmddes,
                      std::string& uuid,
                      const udp::endpoint* udpEndpoint,
                      std::shared_ptr<net::TcpConnection>& tcpConnection,
                      def::ClientInfoHandle handle,
                      def::ClientInfo* clientInfo,
                      net::SendType sendType,
                      uint16_t cmd,
                      uint8_t flags,
                      uint16_t len);
    // void parseCommand(const net::CmdQueueData& cmdData);
    bool loadFromFolder();
    bool createFromConfig();
    bool loadWorld();
    bool saveWorld();
    bool createWorld();
    bool loadMods();
    void update(float dt);
    void postWorldSetup();
    void rerunDebugMovePhy();
    void registerConsoleCommands();
    void runSlowClientDump(long frameTime);
    void handleTcpDisconnect(const std::shared_ptr<net::TcpConnection>& conn);
    void sendAllEnttComponents(const std::shared_ptr<net::TcpConnection>& conn);
    void sendAllComponents(ecs::EntityId entityId,
                           const std::shared_ptr<net::TcpConnection>& conn);
    void testSpawn();
    void handleGetAabbTree(uint32_t sectorId,
                           const std::shared_ptr<net::TcpConnection>& conn);

    const sphy::CmdLinOptionsServer& options;
    std::atomic<bool> stopRequested{false};
    std::thread engineThread;
    con::ItemLib<def::ClientInfo> clientLib;
    std::vector<def::ClientInfoHandle> activeClientHandles;
    mod::ModManager modManager;
    mod::LuaInterpreter luaInterpreter;
    sthread::WorkDistributor workDistributor;

    EngineState state;
    world::World world;
    cfg::ConfigManager saveConfig;
    cfg::ConfigManager& config;
    std::string saveFolder;
    ecs::AssetFactory assetFactory;
    vector<ecs::EntityId> globalEntityIds;
    vector<entt::entity> globalEntities;
    ecs::PtrHandle* ptrHandle;
    rerun::RecordingStream rerunStream;
    cmd::CommandManager commandManager;

    uint32_t slowDumpUs;
    vector<SlowDumpEntry> slowDumpComponents;
    float filteredFps = 0.0f;

  public:
    ecs::Ecs ecs;
};

}  // namespace sphys

#endif