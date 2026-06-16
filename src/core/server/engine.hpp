#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "comp-ident.hpp"
#include "ecs.hpp"
#include "world-def.hpp"
#include <asset-factory.hpp>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/container/vector.hpp>
#include <client-def.hpp>
#include <cmd-options.hpp>
#include <command-node.hpp>
#include <config-manager/config-manager.hpp>
#include <control-def.hpp>
#include <functional>
#include <item-lib.hpp>
#include <lib-hull.hpp>
#include <mod-manager.hpp>
#include <net-shared.hpp>
#include <ptr-handle.hpp>
#include <string>
#include <task-system.hpp>
#include <work-distributor.hpp>
#include <world.hpp>

#include <comp-ai.hpp>
#include <comp-gfx.hpp>
#include <comp-lifetime.hpp>
#include <comp-phy.hpp>
#include <comp-storage.hpp>
#include <comp-struct.hpp>
#include <comp-turret.hpp>
#include <comp-tag.hpp>

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

enum class DumpFilter
{
    All,
    Selectable,
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
    void spawnProjectile(
        uint32_t sectorId,
        vec2 pos,
        vec2 vel,
        gobj::ProjectileHandle projectileHandle,
        const ecs::EntityId& exceptEntity = ecs::EntityId::Invalid(),
        vec2 parVel = vec2(0.0f, 0.0f));
    void spawnAsteroid(uint32_t sectorId,
                       const ecs::Transform& transform,
                       const gobj::AsteroidHandle& asteroidHandle,
                       float rotVel);
    void spawnItem(uint32_t sectorId,
                   const ecs::Transform& transform,
                   const gobj::ItemHandle& itemHandle,
                   float quantity,
                   vec2 initialVel = vec2(0.0f, 0.0f),
                   ecs::EntityId collexcept = ecs::EntityId::Invalid());
    ConcurrentQueue<net::CmdQueueData> sendQueue;
    ConcurrentQueue<net::CmdQueueData> receiveQueue;
    template <class T> void registerSlowDumpComponent();
    template <class T>
    void registerActiveSectorDumpComponent(DumpFilter filter = DumpFilter::All);
    void destroyEntity(ecs::EntityId entityId);

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
                      size_t dataEndPos);
    // void parseCommand(const net::CmdQueueData& cmdData);
    void initPost();
    bool loadFromFolder();
    bool createFromConfig();
    bool loadWorld();
    bool saveWorld();
    bool createWorld();
    bool loadMods();
    void update(float dt);
    void postWorldSetup();
    void registerConsoleCommands();
    void runSlowClientDump(long frameTime);
    void runActiveSectorDump(long frameTime);
    void runConnectedClientWorkSequencers();
    void handleTcpDisconnect(net::TcpConnection* conn,
                             def::ClientInfoHandle disconnectedHandle);
    void sendAllEnttComponents(def::ClientInfo* clientInfo,
                               net::TcpConnection* conn);
    void sendAllComponents(ecs::EntityId entityId, net::TcpConnection* conn);
    void broadcastEntityToClients(ecs::EntityId entityId);
    void testSpawn();
    void handleGetAabbTree(uint32_t sectorId, net::TcpConnection* conn);
    void handleThirdPersonControl(def::ClientInfo* clientInfo,
                                  net::TcpConnection* conn);
    void markPlayerSectors();

    ecs::EntityId spawnShipHull(gobj::HullHandle hullHandle,
                                uint32_t sectorId,
                                const ecs::Transform& transform);
    ecs::EntityId spawnStation(uint32_t sectorId,
                               const ecs::Transform& transform);
    ecs::EntityId addFirstStationPart(ecs::EntityId stationId,
                                      const gobj::StationPartHandle& partHandle,
                                      float rot);
    ecs::EntityId addStationPart(ecs::EntityId stationId,
                                 ecs::EntityId partIdConnectTo,
                                 const gobj::StationPartHandle& partHandle,
                                 uint16_t slotConnectTo,
                                 uint16_t slotNewPart);

    ecs::Hull* makeHull(entt::entity entity,
                        const gobj::HullHandle& hullHandle);
    ecs::Station* makeStation(entt::entity entity);
    ecs::Collider*
    makeCollider(entt::entity entity,
                 const gobj::ColliderHandle& colliderHandle,
                 ecs::CollisionLayer colliderType,
                 const ecs::EntityId& exceptEntity = ecs::EntityId::Invalid());
    ecs::Projectile*
    makeProjectile(entt::entity entity,
                   const gobj::ProjectileHandle& projectileHandle);
    ecs::MapIcon* makeMapIcon(entt::entity entity);
    ecs::SimpleTexture* makeSimpleTexture(entt::entity entity,
                                          const ecs::SimpleTexture& texture);
    ecs::Textures* makeTextures(entt::entity entity,
                                const gobj::TexturesHandle& texturesHandle);
    ecs::PhysicsBody* makePhysicsBody(entt::entity entity,
                                      const ecs::PhysicsBody& physicsBody);
    ecs::MoveCtrl* makeMoveCtrl(entt::entity entity,
                                const ecs::PhyThrust& phyThrust,
                                const ecs::MoveCtrl& moveCtrl);
    ecs::Asteroid* makeAsteroid(entt::entity entity,
                                const gobj::AsteroidHandle& asteroidHandle);
    ecs::Module* makeModule(entt::entity entity, const ecs::Module& module);
    ecs::Item* makeItem(entt::entity entity,
                        const gobj::ItemHandle& itemHandle,
                        float quantity);
    ecs::Storage* makeStorage(entt::entity entity, const ecs::Storage& storage);
    ecs::Lifetime* makeLifetime(entt::entity entity, float lifetime);
    void makeOOSSync(entt::entity entity);
    ecs::StationPart*
    makeStationPart(entt::entity entity,
                    const gobj::StationPartHandle& partHandle);
    ecs::Turret* makeTurret(entt::entity entity,
                            const ecs::Turret& turret,
                            ai::taskdata::Turret defaultTask);
    void makeSelectable(entt::entity entity);
    ecs::AnchorFixed* makeAnchorFixed(entt::entity entity,
                                      const ecs::AnchorFixed& anchorFixed);
    ecs::Ai*
    makeAi(entt::entity entity,
           const ai::taskdata::TaskData& defaultTask = ai::taskdata::Idle());
    bool placeInSector(ecs::EntityId ent,
                       entt::entity entity,
                       uint32_t sectorId,
                       const ecs::Transform& transform);

    ecs::EntityId spawnModule(ecs::EntityId parent,
                              const gobj::ModuleHandle& moduleHandle,
                              uint16_t slotIndex);
    void loadCollisionMatrix();
    void
    forActiveClients(std::function<void(def::ClientInfo* clientInfo)> callback);

    const sphy::CmdLinOptionsServer& options;
    std::atomic<bool> stopRequested{false};
    std::thread engineThread;
    con::ItemLib<def::ClientInfo> clientLib;
    std::vector<def::ClientInfoHandle> connectedClientHandles;
    std::vector<def::ClientInfoHandle> activeClientHandles;
    mod::ModManager modManager;
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
    cmd::CommandManager commandManager;

    uint32_t slowDumpUs;
    uint32_t activeSectorDumpUs;
    vector<CompClientDump> slowDumpComponents;
    vector<CompActiveSectorUpdate> activeSectorUpdates;
    float filteredFps = 0.0f;

    ecs::CollisionLayerMat collisionLayerMat;
    gobj::ColliderHandle itemColliderHandle;

    float itemLifetime;
    ai::TaskSystem taskSystem;

  public:
    ecs::Ecs ecs;
};

}  // namespace sphys

#endif