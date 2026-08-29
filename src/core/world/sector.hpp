#ifndef SECTOR_HPP
#define SECTOR_HPP

#include "entt/entity/fwd.hpp"
#include "free-vector.hpp"
#include <comp-phy.hpp>
#include <cstdint>
#include <ptr-handle.hpp>
#include <std-inc.hpp>
#ifdef CLIENT
#include <render-engine.hpp>
#include <obj-pool-client.hpp>
#include <client-pool-obj.hpp>
#endif
#include <aabb-tree.hpp>
#ifdef SERVER
#include "registry-mapping.hpp"
#include <obj-pool.hpp>
#include <pool-objects.hpp>
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

#ifdef SERVER
enum class BpUserType : uint8_t
{
    Ecs,
    Item
};
union BpUserDataUnion
{
    entt::entity ent;
    opool::ItemHandle itemHandle;
};
struct BpUserData
{
    BpUserType type = BpUserType::Ecs;
    BpUserDataUnion data = {.ent = entt::null};
};
#endif

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
    void destroyBroadphaseProxyId(int32_t proxyId);
    void getAllAABBs(std::vector<con::AABB>& aabbs) const;
    void queryBroadphase(const con::AABB& aabb,
                         std::function<void(const BpUserData&)> callback);
    void queryBroadphasePoint(const vec2& point,
                              std::function<void(const BpUserData&)> callback);
    void markPlayerSector(bool player);
    void update(float dt, ecs::PtrHandle* ptrHandle);
    bool saveSector(const std::string& savedir);
    template <class T>
    void foreachOpool(std::function<con::FreeVecForeachRet(
                          T&,
                          typename con::FreeVec<T>::Handle handle)> clb);

    template <class T> T* getOpool(typename con::FreeVec<T>::Handle handle);
    void objectInitBroadphase(ecs::PtrHandle* ptrHandle, entt::entity entity);
    void itemInitBroadphase(ecs::PtrHandle* ptrHandle,
                            opool::ItemHandle handle,
                            opool::Item& item);
    void itemUpdateBroadphase(ecs::PtrHandle* ptrHandle,
                              opool::ItemHandle handle,
                              opool::Item& item);
    ecs::SectorRegistry* getRegistry()
    {
        return &sectorRegistry;
    }
    ai::TaskSystem& getTaskSystem()
    {
        return taskSystem;
    }
    template <class T>
    typename con::FreeVec<T>::Handle spawnOpool(ecs::PtrHandle* ptrHandle,
                                                const T& item);
    template <class T>
    void removeOpool(typename con::FreeVec<T>::Handle handle);
    inline void addBroadphaseQueryEntity(entt::entity entity)
    {
        broadphaseQueryEntities.push_back(entity);
    }
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
#ifdef CLIENT
    opool::OpoolClient<opool::ProjClient> projectiles;
    opool::OpoolClient<opool::BeamClient> beams;
    opool::OpoolClient<opool::ItemClient> items;
#endif

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
    con::DynamicAABBTree<BpUserData> aabbTree;
    opool::ObjectPool<opool::Projectile> projectilePool;
    opool::ObjectPool<opool::Item> itemPool;
    opool::ObjectPool<opool::Beam> beamPool;
    ai::TaskSystem taskSystem;
#endif
    bool active = false;
};

#ifdef SERVER

template <>
void Sector::foreachOpool<opool::Projectile>(
    std::function<con::FreeVecForeachRet(opool::Projectile&,
                                         opool::ProjectileHandle handle)> clb)
{
    projectilePool.foreach (clb);
}

template <>
void Sector::foreachOpool<opool::Item>(
    std::function<con::FreeVecForeachRet(opool::Item&,
                                         opool::ItemHandle handle)> clb)
{
    itemPool.foreach (clb);
}

template <>
void Sector::foreachOpool<opool::Beam>(
    std::function<con::FreeVecForeachRet(opool::Beam&,
                                         opool::BeamHandle handle)> clb)
{
    beamPool.foreach (clb);
}

template <> opool::Item* Sector::getOpool<opool::Item>(opool::ItemHandle handle)
{
    return itemPool.getObject(handle);
}

template <>
opool::Projectile*
Sector::getOpool<opool::Projectile>(opool::ProjectileHandle handle)
{
    return projectilePool.getObject(handle);
}

template <> opool::Beam* Sector::getOpool<opool::Beam>(opool::BeamHandle handle)
{
    return beamPool.getObject(handle);
}

template <> void Sector::removeOpool<opool::Beam>(opool::BeamHandle handle)
{
    beamPool.destroyObject(handle);
}

template <> void Sector::removeOpool<opool::Item>(opool::ItemHandle handle)
{
    itemPool.destroyObject(handle);
}

template <>
void Sector::removeOpool<opool::Projectile>(opool::ProjectileHandle handle)
{
    projectilePool.destroyObject(handle);
}

template <>
opool::ItemHandle Sector::spawnOpool<opool::Item>(ecs::PtrHandle* ptrHandle,
                                                  const opool::Item& item)
{
    auto handle = itemPool.spawnObject(item);
    auto* i = itemPool.getObject(handle);
    if (i)
    {
        itemInitBroadphase(ptrHandle, handle, *i);
    }
    return handle;
}

template <>
opool::ProjectileHandle
Sector::spawnOpool<opool::Projectile>(ecs::PtrHandle* ptrHandle,
                                      const opool::Projectile& proj)
{
    return projectilePool.spawnObject(proj);
}

template <>
opool::BeamHandle Sector::spawnOpool<opool::Beam>(ecs::PtrHandle* ptrHandle,
                                                  const opool::Beam& beam)
{
    return beamPool.spawnObject(beam);
}

#endif

}  // namespace world

#endif