#ifndef SECTOR_HPP
#define SECTOR_HPP

#include <ecs.hpp>
#include <ptr-handle.hpp>
#include <std-inc.hpp>
#ifdef CLIENT
#include <render-engine.hpp>
#endif
#include <aabb-tree.hpp>

namespace world
{

const uint32_t INVALID_SECTOR_ID = 0xFFFFFFFF;

class Sector
{
  public:
    Sector();
    ~Sector();
    void
    init(int x, int y, float sectorSize, uint32_t id, Sector* neighbors[8]);
    bool saveSector(const std::string& savedir);
    void update(float dt, std::shared_ptr<ecs::PtrHandle> ptrHandle);
    bool addEntity(std::shared_ptr<ecs::PtrHandle> ptrHandle,
                   ecs::EntityId entityId);
    bool removeEntity(std::shared_ptr<ecs::PtrHandle> ptrHandle,
                      ecs::EntityId entityId);
    void moveAabbProxy(int32_t proxyId, con::AABB& newAabb);
    void getAllAABBs(std::vector<con::AABB>& aabbs) const;
    void queryBroadphase(const con::AABB& aabb,
                         std::function<void(entt::entity)> callback);
    vec2 getWorldPosSectorOffset(int32_t sectorOffsetX,
                                 int32_t sectorOffsetY) const;
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
    const vector<entt::entity>& getEntities() const
    {
        return entities;
    }
    const vector<ecs::EntityId>& getEntityIds() const
    {
        return entityIds;
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

    vector<ecs::EntityId> entityIds;
    vector<entt::entity> entities;
    con::DynamicAABBTree<entt::entity> aabbTree;
};

}  // namespace world

#endif