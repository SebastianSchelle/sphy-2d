#include "entt/entity/fwd.hpp"
#include <comp-ident.hpp>
#include <comp-phy.hpp>
#include <ptr-handle.hpp>
#include <sector.hpp>
#include <type_traits>
#include <variant>

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

void Sector::update(float dt, ecs::PtrHandle* ptrHandle)
{
    broadphaseQueryEntities.clear();

    for (const auto& system : *ptrHandle->systems)
    {
        if (system.type == ecs::SystemType::SectorOnce && !system.afterEntityUpdate)
        {
            std::visit(
                [&](auto&& fn)
                {
                    using Fn = std::decay_t<decltype(fn)>;
                    if constexpr (std::is_same_v<Fn, ecs::SFSectorOnce>)
                    {
                        fn(this, dt, ptrHandle);
                    }
                },
                system.function);
        }
    }

    for (size_t i = 0; i < entityIds.size(); i++)
    {
        for (const auto& system : *ptrHandle->systems)
        {
            if (system.type == ecs::SystemType::SectorForeachEntitiy)
            {
                std::visit(
                    [&](auto&& fn)
                    {
                        using Fn = std::decay_t<decltype(fn)>;
                        if constexpr (std::is_same_v<Fn, ecs::SFSectorForeach>)
                        {
                            fn(this, entities[i], entityIds[i], dt, ptrHandle);
                        }
                    },
                    system.function);
            }
        }
    }

    for(const auto& system : *ptrHandle->systems)
    {
        if (system.type == ecs::SystemType::SectorOnce && system.afterEntityUpdate)
        {
            std::visit(
                [&](auto&& fn)
                {
                    using Fn = std::decay_t<decltype(fn)>;
                    if constexpr (std::is_same_v<Fn, ecs::SFSectorOnce>)
                    {
                        fn(this, dt, ptrHandle);
                    }
                },
                system.function);
        }
    }
}

bool Sector::addEntity(ecs::PtrHandle* ptrHandle,
                       ecs::EntityId entityId)
{
    if (!ptrHandle->ecs->validId(entityId))
    {
        LG_W("Entity not valid: {}", entityId);
        return false;
    }
    auto reg = ptrHandle->registry;
    auto it = std::find(entityIds.begin(), entityIds.end(), entityId);
    if (it != entityIds.end())
    {
        LG_W("Entity already in sector: {}", entityId);
        return false;
    }
    entt::entity entity = ptrHandle->ecs->getEntity(entityId);
    entityIds.push_back(entityId);
    entities.push_back(entity);
    auto& sector = reg->get<ecs::SectorId>(entity);
    sector.id = id;
    sector.x = (uint32_t)coordX;
    sector.y = (uint32_t)coordY;
    // add AABB calculation from polygon
    auto& transform = reg->get<ecs::Transform>(entity);
    auto* transformCache = reg->try_get<ecs::TransformCache>(entity);
    auto* collider = reg->try_get<ecs::Collider>(entity);
    auto* broadphase = reg->try_get<ecs::Broadphase>(entity);
    if (transformCache && collider && broadphase)
    {
        transformCache->c = cosf(transform.rot);
        transformCache->s = sinf(transform.rot);
        con::AABB aabb =
            ecs::calculateAABB(transform, *transformCache, *collider);
        broadphase->proxyId = aabbTree.createProxy(aabb, entity);
        broadphase->fatAABB = aabb;
    }
    return true;
}

bool Sector::removeEntity(ecs::PtrHandle* ptrHandle,
                          ecs::EntityId entityId)
{
    if (!ptrHandle->ecs->validId(entityId))
    {
        LG_W("Entity not valid: {}", entityId);
        return false;
    }
    auto reg = ptrHandle->registry;
    auto it = std::find(entityIds.begin(), entityIds.end(), entityId);
    if (it == entityIds.end())
    {
        LG_W("Entity not in sector: {}", entityId);
        return false;
    }
    entityIds.erase(it);
    entities.erase(std::find(
        entities.begin(), entities.end(), ptrHandle->ecs->getEntity(entityId)));
    auto& sector = reg->get<ecs::SectorId>(ptrHandle->ecs->getEntity(entityId));
    sector.id = INVALID_SECTOR_ID;
    sector.x = 0;
    sector.y = 0;
    auto broadphase =
        reg->try_get<ecs::Broadphase>(ptrHandle->ecs->getEntity(entityId));
    if (broadphase)
    {
        aabbTree.destroyProxy(broadphase->proxyId);
    }
    return true;
}

vec2 Sector::getWorldPosSectorOffset(int32_t sectorOffsetX,
                                     int32_t sectorOffsetY) const
{
    return vec2((float)(coordX - sectorOffsetX) * sectorSize,
                (float)(coordY - sectorOffsetY) * sectorSize);
}

void Sector::moveAabbProxy(int32_t proxyId, con::AABB& newAabb)
{
    aabbTree.moveProxy(proxyId, newAabb);
}

void Sector::getAllAABBs(std::vector<con::AABB>& aabbs) const
{
    aabbTree.getAllAABBs(aabbs);
}

void Sector::queryBroadphase(const con::AABB& aabb, std::function<void(entt::entity)> callback)
{
    aabbTree.query(aabb, callback);
}

void Sector::markPlayerSector(bool player)
{
    playerSector = player;
}

#ifdef CLIENT
void Sector::drawDebug(gfx::RenderEngine& renderer, float zoom)
{
    int32_t sectorOffsetX = renderer.getSectorOffsetX();
    int32_t sectorOffsetY = renderer.getSectorOffsetY();
    glm::vec2 pos = getWorldPosSectorOffset(sectorOffsetX, sectorOffsetY);
    renderer.drawRectangle(
        pos, glm::vec2(sectorSize, sectorSize), 0x10aaaa00, 1.0f / zoom);
}

void Sector::drawTacticalMap(gfx::RenderEngine& renderer,
                             const glm::vec4& viewRect,
                             float zoom)
{
}

void Sector::drawStrategicMap(gfx::RenderEngine& renderer,
                              const glm::vec4& viewRect,
                              float zoom)
{
    int32_t sectorOffsetX = renderer.getSectorOffsetX();
    int32_t sectorOffsetY = renderer.getSectorOffsetY();
    glm::vec2 pos = getWorldPosSectorOffset(sectorOffsetX, sectorOffsetY);
    renderer.drawRectangle(
        pos, glm::vec2(sectorSize, sectorSize), 0x10aaaa00, 1.0f / zoom);
}

void Sector::drawThirdPerson(gfx::RenderEngine& renderer,
                             const glm::vec4& viewRect,
                             float zoom)
{
}


#endif

}  // namespace world