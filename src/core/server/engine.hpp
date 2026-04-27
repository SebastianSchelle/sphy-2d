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

#include <lib-hull.hpp>

#include <comp-gfx.hpp>
#include <comp-phy.hpp>
#include <comp-struct.hpp>


namespace ecs
{
struct PtrHandle;
}

namespace sphys
{

typedef std::function<void(const net::ClientInfo* clientInfo,
                           ecs::PtrHandle* ptrHandle)>
    ClientDumpFunction;
typedef std::function<void(const net::ClientInfo* clientInfo,
                           uint32_t sectorId,
                           ecs::PtrHandle* ptrHandle)>
    ActiveSectorUpdateFunction;
struct CompClientDump
{
    string componentName;
    ClientDumpFunction function;
};
struct CompActiveSectorUpdate
{
    string componentName;
    ActiveSectorUpdateFunction function;
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
    template <class T> void registerActiveSectorDumpComponent();

  private:
    void engineLoop();
    void startFromFolder();
    void parseCommandData(const net::CmdQueueData& cmdData);
    void parseCommand(bitsery::Deserializer<InputAdapter>& cmddes,
                      std::string& uuid,
                      const udp::endpoint* udpEndpoint,
                      net::TcpConnection* tcpConnection,
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
    void runActiveSectorDump(long frameTime);
    void runConnectedClientWorkSequencers();
    void handleTcpDisconnect(net::TcpConnection* conn,
                             def::ClientInfoHandle disconnectedHandle);
    void sendAllEnttComponents(def::ClientInfo* clientInfo,
                               net::TcpConnection* conn);
    void sendAllComponents(ecs::EntityId entityId, net::TcpConnection* conn);
    void testSpawn();
    void handleGetAabbTree(uint32_t sectorId, net::TcpConnection* conn);
    void markPlayerSectors();

    ecs::EntityId spawnShipHull(gobj::HullHandle hullHandle,
                                uint32_t sectorId,
                                const ecs::Transform& transform);

    ecs::Hull* makeHull(entt::entity entity,
                        const gobj::HullHandle& hullHandle);
    ecs::Collider* makeCollider(entt::entity entity,
                                const gobj::ColliderHandle& colliderHandle);
    ecs::MapIcon* makeMapIcon(entt::entity entity,
                              const gobj::MapIconHandle& mapIconHandle);
    ecs::Textures* makeTextures(entt::entity entity,
                                const gobj::TexturesHandle& texturesHandle);
    ecs::PhysicsBody* makePhysicsBody(entt::entity entity,
                                      const ecs::PhysicsBody& physicsBody);
    ecs::MoveCtrl* makeMoveCtrl(entt::entity entity,
                                const ecs::PhyThrust& phyThrust,
                                const ecs::MoveCtrl& moveCtrl);
    bool placeInSector(ecs::EntityId ent,
                       entt::entity entity,
                       uint32_t sectorId,
                       const ecs::Transform& transform);

    ecs::EntityId spawnModule(ecs::EntityId parent);

    const sphy::CmdLinOptionsServer& options;
    std::atomic<bool> stopRequested{false};
    std::thread engineThread;
    con::ItemLib<def::ClientInfo> clientLib;
    std::vector<def::ClientInfoHandle> connectedClientHandles;
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
    uint32_t activeSectorDumpUs;
    vector<CompClientDump> slowDumpComponents;
    vector<CompActiveSectorUpdate> activeSectorUpdates;
    float filteredFps = 0.0f;

  public:
    ecs::Ecs ecs;
};

}  // namespace sphys

#endif