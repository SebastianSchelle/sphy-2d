#include "entt/entity/fwd.hpp"
#include <comp-ident.hpp>
#include <ptr-handle.hpp>
#include <sector.hpp>

namespace world
{

Sector::Sector() {}

Sector::~Sector() {}

void Sector::init(int x,
                  int y,
                  float sectorSize,
                  uint32_t id,
                  Sector* neighbors[8])
{
    coordX = x;
    coordY = y;
    this->sectorSize = sectorSize;
    worldPosX = coordX * sectorSize;
    worldPosY = coordY * sectorSize;
    this->id = id;
    for (int i = 0; i < 8; i++)
    {
        this->neighbors[i] = neighbors[i];
    }
    dirty = true;
}

bool Sector::saveSector(const std::string& savedir)
{
    if (dirty)
    {
        std::string sectorSaveFld = savedir + "/save-data/world";
        std::string sectorFilePath = savedir + "/sector-" + std::to_string(id)
                                     + "." + GAME_NAME + ".sav";
    }
    return true;
}

void Sector::update(float dt, std::shared_ptr<ecs::PtrHandle> ptrHandle)
{
    for (auto entity : entityIds)
    {
        for (auto system : *ptrHandle->systems)
        {
            system.function(entity, dt, ptrHandle);
        }
    }
}

bool Sector::addEntity(std::shared_ptr<ecs::PtrHandle> ptrHandle,
                       ecs::EntityId entityId)
{
    if (!ptrHandle->ecs->validId(entityId))
    {
        LG_W("Entity not valid: {}", entityId);
        return false;
    }
    auto reg = ptrHandle->registry;
    auto it = std::find(entities.begin(), entities.end(), entityId);
    if (it != entities.end())
    {
        LG_W("Entity already in sector: {}", entityId);
        return false;
    }
    entities.push_back(entityId);
    entityIds.push_back(ptrHandle->ecs->getEntity(entityId));
    reg->emplace_or_replace<ecs::SectorId>(ptrHandle->ecs->getEntity(entityId),
                                           ecs::SectorId{id});
    LG_D("Added entity: {} to sector: {}", entityId, id);
    return true;
}

bool Sector::removeEntity(std::shared_ptr<ecs::PtrHandle> ptrHandle,
                          ecs::EntityId entityId)
{
    if (ptrHandle->ecs->validId(entityId))
    {
        LG_W("Entity not valid: {}", entityId);
        return false;
    }
    auto reg = ptrHandle->registry;
    auto it = std::find(entities.begin(), entities.end(), entityId);
    if (it == entities.end())
    {
        LG_W("Entity not in sector: {}", entityId);
        return false;
    }
    entities.erase(it);
    entityIds.erase(std::find(entityIds.begin(),
                              entityIds.end(),
                              ptrHandle->ecs->getEntity(entityId)));
    reg->emplace_or_replace<ecs::SectorId>(ptrHandle->ecs->getEntity(entityId),
                                           ecs::SectorId{0xFFFFFFFF});
    LG_D("Removed entity: {} from sector: {}", entityId, id);
    return true;
}

vec2 Sector::getWorldPosSectorOffset(int32_t sectorOffsetX,
                                     int32_t sectorOffsetY) const
{
    return vec2((float)(coordX - sectorOffsetX) * sectorSize,
                (float)(coordY - sectorOffsetY) * sectorSize);
}

#ifdef CLIENT
void Sector::drawDebug(gfx::RenderEngine& renderer, float zoom)
{
    int32_t sectorOffsetX = renderer.getSectorOffsetX();
    int32_t sectorOffsetY = renderer.getSectorOffsetY();
    glm::vec2 pos = getWorldPosSectorOffset(sectorOffsetX, sectorOffsetY);
    renderer.drawRectangle(pos,
                           glm::vec2(sectorSize, sectorSize),
                           0x10aaaa00,
                           1.0f / zoom);
}
#endif

}  // namespace world