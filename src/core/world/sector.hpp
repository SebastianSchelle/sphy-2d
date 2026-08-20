#ifndef SECTOR_HPP
#define SECTOR_HPP

#include <comp-phy.hpp>
#include <ptr-handle.hpp>
#include <std-inc.hpp>
#ifdef CLIENT
#include <render-engine.hpp>
#endif
#include <aabb-tree.hpp>
#include <unordered_set>
#ifdef SERVER
#include "registry-mapping.hpp"
#include <projectile.hpp>
#include <sector-registry.hpp>
#include <task-system.hpp>
#endif

namespace world
{

typedef std::function<void(ecs::PtrHandle* ptrHandle)>
    SingleThreadedTaskFunction;

struct SectorMoveRequest
{
    ecs::EntityId entityId;
    uint32_t newSectorId;
};

class Sector
{
  public:
    struct EntRef
    {
        ecs::EntityId entityId;
        entt::entity entity;
    };

    Sector();
    ~Sector();
    void init(int x,
              int y,
              float sectorSize,
              uint32_t id,
              Sector* neighbors[8],
              ecs::RegistryMapping* regMapping);
    vec2 getWorldPosSectorOffset(int32_t sectorOffsetX,
                                 int32_t sectorOffsetY) const;
#ifdef SERVER
    ecs::EntityId spawnObject(ecs::PtrHandle* ptrHandle,
                              const ecs::SpawnCallback& spwnClb);
    bool migrateObject(ecs::PtrHandle* ptrHandle, ecs::EntityId entityId);
    bool removeEntity(ecs::PtrHandle* ptrHandle, ecs::EntityId entityId);
    void markEntityForDestruction(ecs::PtrHandle* ptrHandle,
                                  ecs::EntityId entityId);
    void destroyMarkedEntities(ecs::PtrHandle* ptrHandle);
    void addSingleThreadedTask(SingleThreadedTaskFunction task);
    void executeSingleThreadedTasks(ecs::PtrHandle* ptrHandle);
    void addSectorMoveRequest(ecs::PtrHandle* ptrHandle,
                              const SectorMoveRequest& request);
    void forSectorMoveRequests(
        std::function<void(const SectorMoveRequest& request)> callback);
    void moveAabbProxy(int32_t proxyId, con::AABB& newAabb);
    void destroyBroadphaseProxy(ecs::Broadphase* broadphase);
    void getAllAABBs(std::vector<con::AABB>& aabbs) const;
    void queryBroadphase(const con::AABB& aabb,
                         std::function<void(entt::entity)> callback);
    void markPlayerSector(bool player);
    void update(float dt, ecs::PtrHandle* ptrHandle);
    bool saveSector(const std::string& savedir);
    void foreachProj(
        std::function<con::FreeVecForeachRet(specsys::Projectile&,
                                             specsys::ProjectileHandle handle)>
            clb);
#endif
    const float getWorldPosX() const
    {
        return worldPosX;
    }
    const float getWorldPosY() const
    {
        return worldPosY;
    }
    const glm::vec2 getWorldPos() const
    {
        return glm::vec2(worldPosX, worldPosY);
    }
    const uint32_t getId() const
    {
        return id;
    }
    const uint32_t getCoordX()
    {
        return coordX;
    }
    const uint32_t getCoordY()
    {
        return coordY;
    }
    bool isActive()
    {
        return active;
    }
    void objectInitBroadphase(ecs::PtrHandle* ptrHandle, entt::entity entity);
#ifdef SERVER
    ecs::SectorRegistry* getRegistry()
    {
        return &sectorRegistry;
    }
    ai::TaskSystem& getTaskSystem()
    {
        return taskSystem;
    }
    void spawnProjectile(gobj::ProjectileHandle proj,
                         vec2 vel,
                         const ecs::Transform& tr,
                         float lifetime,
                         ecs::EntityId collExcept = ecs::EntityId::Invalid());
#endif
    inline void addBroadphaseQueryEntity(entt::entity entity)
    {
        broadphaseQueryEntities.push_back(entity);
    }
#ifdef CLIENT
    void drawDebug(gfx::RenderEngine& renderer, float zoom);
    void drawTacticalMap(gfx::RenderEngine& renderer,
                         const glm::vec4& viewRect,
                         float zoom);
    void drawStrategicMap(gfx::RenderEngine& renderer,
                          const glm::vec4& viewRect,
                          float zoom);
    void drawThirdPerson(gfx::RenderEngine& renderer,
                         const glm::vec4& viewRect,
                         float zoom);
#endif

    std::vector<std::pair<entt::entity, entt::entity>> broadphaseCollisions;
    std::vector<entt::entity> broadphaseQueryEntities;
    vector<ecs::ContactInfo> contactInfos;

  private:
    int32_t coordX;        // Sector coord X
    int32_t coordY;        // Sector coord Y
    float sectorSize;      // Sector size
    uint32_t id;           // Sector Id
    float worldPosX;       // Sector center X in world coords
    float worldPosY;       // Sector center Y in world coords
    Sector* neighbors[8];  // Neighboring Sectors (8 neighbors)
    bool dirty;            // Sector dirty flag

#ifdef SERVER
    ecs::SectorRegistry sectorRegistry;
    vector<ecs::EntityId> entitiesToDestroy;
    vector<SingleThreadedTaskFunction> singleThreadedTasks;
    vector<SectorMoveRequest> sectorMoveRequests;
    specsys::ProjectilePool projectilePool;
#endif
    con::DynamicAABBTree<entt::entity> aabbTree;
    bool active = false;
#ifdef SERVER
    ai::TaskSystem taskSystem;
#endif
};

}  // namespace world

#endif