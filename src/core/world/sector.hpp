#ifndef SECTOR_HPP
#define SECTOR_HPP

#include <ecs.hpp>
#include <ptr-handle.hpp>
#include <std-inc.hpp>
#ifdef CLIENT
#include <render-engine.hpp>
#endif
#include <aabb-tree.hpp>
#include <unordered_set>
#ifdef SERVER
#include <task-system.hpp>
#endif

namespace world
{

const uint32_t INVALID_SECTOR_ID = 0xFFFFFFFF;

typedef std::function<void(ecs::PtrHandle* ptrHandle)> SingleThreadedTaskFunction;

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
        static constexpr uint8_t FLAG_DESTROYED = 1 << 0;
        static constexpr uint8_t FLAG_MOVED = 1 << 1;
        ecs::EntityId entityId;
        entt::entity entity;
        uint8_t flags = 0;
    };

    Sector();
    ~Sector();
    void
    init(int x, int y, float sectorSize, uint32_t id, Sector* neighbors[8]);
    bool saveSector(const std::string& savedir);
    void update(float dt, ecs::PtrHandle* ptrHandle);
    bool addEntity(ecs::PtrHandle* ptrHandle, ecs::EntityId entityId);
    bool removeEntity(ecs::PtrHandle* ptrHandle, ecs::EntityId entityId);
    void moveAabbProxy(int32_t proxyId, con::AABB& newAabb);
    void destroyBroadphaseProxy(ecs::Broadphase* broadphase);
    void getAllAABBs(std::vector<con::AABB>& aabbs) const;
    void queryBroadphase(const con::AABB& aabb,
                         std::function<void(entt::entity)> callback);
    vec2 getWorldPosSectorOffset(int32_t sectorOffsetX,
                                 int32_t sectorOffsetY) const;
    void markPlayerSector(bool player);
#ifdef SERVER
    void markEntityForDestruction(ecs::EntityId entityId);
    void destroyMarkedEntities(ecs::PtrHandle* ptrHandle);
    void addSingleThreadedTask(SingleThreadedTaskFunction task);
    void executeSingleThreadedTasks(ecs::PtrHandle* ptrHandle);
    void addSectorMoveRequest(const SectorMoveRequest& request);
    void forSectorMoveRequests(std::function<void(const SectorMoveRequest& request)> callback);
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
#ifdef SERVER
    ai::TaskSystem& getTaskSystem()
    {
        return taskSystem;
    }
#endif
    const vector<EntRef>& getEntityRefs() const
    {
        return entityRefs;
    }
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
    void visitEntityRef(ecs::EntityId entityId,
                        std::function<void(EntRef& ref)> callback);
    void visitEntityRef(entt::entity entity,
                        std::function<void(EntRef& ref)> callback);

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

    vector<EntRef> entityRefs;
#ifdef SERVER
    vector<ecs::EntityId> entitiesToDestroy;
    vector<SingleThreadedTaskFunction> singleThreadedTasks;
    vector<SectorMoveRequest> sectorMoveRequests;
#endif
    con::DynamicAABBTree<entt::entity> aabbTree;
    bool playerSector = false;
#ifdef SERVER
    ai::TaskSystem taskSystem;
#endif
};

}  // namespace world

#endif