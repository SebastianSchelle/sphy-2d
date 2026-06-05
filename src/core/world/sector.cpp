#include "entt/entity/fwd.hpp"
#include <comp-ident.hpp>
#include <comp-phy.hpp>
#include <ptr-handle.hpp>
#include <sector.hpp>
#include <type_traits>
#include <variant>
#ifdef SERVER
#include <engine.hpp>
#endif

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
        if (system.type == ecs::SystemType::SectorEarly)
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

    for (const auto& entityRef : entityRefs)
    {
        for (const auto& system : *ptrHandle->systems)
        {
            std::visit(
                [&](auto&& fn)
                {
                    using Fn = std::decay_t<decltype(fn)>;
                    if constexpr (std::is_same_v<Fn, ecs::SFSectorForeach>)
                    {
                        if (entityRef.flags & EntRef::FLAG_DESTROYED)
                        {
                            return;
                        }
                        fn(this,
                           entityRef.entity,
                           entityRef.entityId,
                           dt,
                           ptrHandle);
                    }
                },
                system.function);
        }
    }

    for (const auto& system : *ptrHandle->systems)
    {
        if (system.type == ecs::SystemType::SectorLate)
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

bool Sector::addEntity(ecs::PtrHandle* ptrHandle, ecs::EntityId entityId)
{
    if (!ptrHandle->ecs->validId(entityId))
    {
        LG_W("Entity not valid: {}", entityId);
        return false;
    }
    auto reg = ptrHandle->registry;
    auto it = std::find_if(entityRefs.begin(),
                           entityRefs.end(),
                           [entityId](const EntRef& ref)
                           { return ref.entityId == entityId; });
    if (it != entityRefs.end())
    {
        LG_W("Entity already in sector: {}", entityId);
        return false;
    }
    entt::entity entity = ptrHandle->ecs->getEntity(entityId);
    entityRefs.push_back(EntRef{entityId, entity, 0});
    auto& sector = reg->get<ecs::SectorId>(entity);
    sector.id = id;
    sector.x = (uint32_t)coordX;
    sector.y = (uint32_t)coordY;
    // add AABB calculation from polygon
    auto& transform = reg->get<ecs::Transform>(entity);
    auto* collider = reg->try_get<ecs::Collider>(entity);
    auto* broadphase = reg->try_get<ecs::Broadphase>(entity);
    if (collider && broadphase)
    {
        auto* transformCache = reg->try_get<ecs::TransformCache>(entity);
        float c = cosf(transform.rot);
        float s = sinf(transform.rot);
        if (transformCache)
        {
            transformCache->c = c;
            transformCache->s = s;
        }
        con::AABB aabb = ecs::calculateAABB(
            transform, {c, s}, *collider, ptrHandle->colliderLib);
        if (broadphase->proxyId <= ecs::Broadphase::INVALID_PROXY_ID)
        {
            broadphase->proxyId = aabbTree.createProxy(aabb, entity);
            broadphase->fatAABB = aabb;
        }
    }
    return true;
}

void Sector::destroyBroadphaseProxy(ecs::Broadphase* broadphase)
{
    if (!broadphase)
    {
        LG_W("broadphase is null");
        return;
    }
    aabbTree.destroyProxy(broadphase->proxyId);
    broadphase->proxyId = ecs::Broadphase::INVALID_PROXY_ID;
}

bool Sector::removeEntity(ecs::PtrHandle* ptrHandle, ecs::EntityId entityId)
{
    if (!ptrHandle->ecs->validId(entityId))
    {
        // LG_W("Entity not valid: {}", entityId);
        return false;
    }
    auto reg = ptrHandle->registry;
    auto it = std::find_if(entityRefs.begin(),
                           entityRefs.end(),
                           [entityId](const EntRef& ref)
                           { return ref.entityId == entityId; });
    if (it == entityRefs.end())
    {
        LG_W("Entity not in sector: {}", entityId);
        return false;
    }
    entt::entity entity = it->entity;
    auto* broadphase = reg->try_get<ecs::Broadphase>(entity);
    if (broadphase)
    {
        destroyBroadphaseProxy(broadphase);
    }
    entityRefs.erase(it);
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
    if (proxyId <= ecs::Broadphase::INVALID_PROXY_ID)
    {
        return;
    }
    aabbTree.moveProxy(proxyId, newAabb);
}

void Sector::getAllAABBs(std::vector<con::AABB>& aabbs) const
{
    aabbTree.getAllAABBs(aabbs);
}

void Sector::queryBroadphase(const con::AABB& aabb,
                             std::function<void(entt::entity)> callback)
{
    aabbTree.query(aabb, callback);
}

void Sector::markPlayerSector(bool player)
{
    playerSector = player;
}

#ifdef SERVER

void Sector::markEntityForDestruction(ecs::EntityId entityId)
{
    visitEntityRef(
        entityId,
        [this, entityId](EntRef& ref)
        {
            if (ref.flags & EntRef::FLAG_DESTROYED)
            {
                return;
            }
            if (ref.flags & EntRef::FLAG_MOVED)
            {
                sectorMoveRequests.erase(
                    std::remove_if(sectorMoveRequests.begin(),
                                   sectorMoveRequests.end(),
                                   [=](const SectorMoveRequest& request)
                                   { return request.entityId == entityId; }),
                    sectorMoveRequests.end());
            }
            ref.flags |= EntRef::FLAG_DESTROYED;
        });
    entitiesToDestroy.push_back(entityId);
}

void Sector::destroyMarkedEntities(ecs::PtrHandle* ptrHandle)
{
    for (const auto& entityId : entitiesToDestroy)
    {
        ptrHandle->engine->destroyEntity(entityId);
    }
    entitiesToDestroy.clear();
}

void Sector::addSingleThreadedTask(SingleThreadedTaskFunction task)
{
    singleThreadedTasks.push_back(task);
}

void Sector::executeSingleThreadedTasks(ecs::PtrHandle* ptrHandle)
{
    for (const auto& task : singleThreadedTasks)
    {
        task(ptrHandle);
    }
    singleThreadedTasks.clear();
}

void Sector::addSectorMoveRequest(const SectorMoveRequest& request)
{
    visitEntityRef(request.entityId,
                   [=](EntRef& ref)
                   {
                       if (ref.flags & EntRef::FLAG_MOVED
                           || ref.flags & EntRef::FLAG_DESTROYED)
                       {
                           return;
                       }
                       ref.flags |= EntRef::FLAG_MOVED;
                   });
    sectorMoveRequests.push_back(request);
}

void Sector::forSectorMoveRequests(
    std::function<void(const SectorMoveRequest& request)> callback)
{
    for (const auto& request : sectorMoveRequests)
    {
        callback(request);
    }
    sectorMoveRequests.clear();
}

#endif

#ifdef CLIENT
void Sector::drawDebug(gfx::RenderEngine& renderer, float zoom)
{
    int32_t sectorOffsetX = renderer.getSectorOffsetX();
    int32_t sectorOffsetY = renderer.getSectorOffsetY();
    glm::vec2 pos = getWorldPosSectorOffset(sectorOffsetX, sectorOffsetY);
    renderer.drawShapeRectangle(
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
    renderer.drawShapeRectangle(
        pos, glm::vec2(sectorSize, sectorSize), 0x10aaaa00, 1.0f / zoom);
}

void Sector::drawThirdPerson(gfx::RenderEngine& renderer,
                             const glm::vec4& viewRect,
                             float zoom)
{
}

#endif

void Sector::visitEntityRef(ecs::EntityId entityId,
                            std::function<void(EntRef& ref)> callback)
{
    auto it = std::find_if(entityRefs.begin(),
                           entityRefs.end(),
                           [entityId](const EntRef& ref)
                           { return ref.entityId == entityId; });
    if (it != entityRefs.end())
    {
        callback(*it);
    }
}

void Sector::visitEntityRef(entt::entity entity,
                            std::function<void(EntRef& ref)> callback)
{
    auto it = std::find_if(entityRefs.begin(),
                           entityRefs.end(),
                           [entity](const EntRef& ref)
                           { return ref.entity == entity; });
    if (it != entityRefs.end())
    {
        callback(*it);
    }
}

}  // namespace world