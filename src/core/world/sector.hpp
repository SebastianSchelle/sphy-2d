#ifndef SECTOR_HPP
#define SECTOR_HPP

#include <ecs.hpp>
#include <std-inc.hpp>
#include <ptr-handle.hpp>
#ifdef CLIENT
#include <render-engine.hpp>
#endif

namespace world
{

class Sector
{
  public:
    Sector();
    ~Sector();
    void
    init(int x, int y, float sectorSize, uint32_t id, Sector* neighbors[8]);
    bool saveSector(const std::string& savedir);
    void update(float dt, std::shared_ptr<ecs::PtrHandle> ptrHandle);
    bool addEntity(std::shared_ptr<ecs::PtrHandle> ptrHandle, ecs::EntityId entityId);
    bool removeEntity(std::shared_ptr<ecs::PtrHandle> ptrHandle, ecs::EntityId entityId);
    const float getWorldPosX() const { return worldPosX; }
    const float getWorldPosY() const { return worldPosY; }
#ifdef CLIENT
    void drawDebug(gfx::RenderEngine& renderer, float zoom);
#endif

  private:
    int coordX;            // Sector coord X
    int coordY;            // Sector coord Y
    float sectorSize;      // Sector size
    uint32_t id;           // Sector Id
    float worldPosX;       // Sector center X in world coords
    float worldPosY;       // Sector center Y in world coords
    Sector* neighbors[8];  // Neighboring Sectors (8 neighbors)
    bool dirty;            // Sector dirty flag

    vector<ecs::EntityId> entities;
    vector<entt::entity> entityIds;
};

}  // namespace world

#endif