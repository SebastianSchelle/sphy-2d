#include "comp-tag.hpp"
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
                  Sector* neighbors[8],
                  ecs::RegistryMapping* regMapping)
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
#ifdef SERVER
    sectorRegistry.init(regMapping, this);
#endif
}


/*
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
    entityRefs.push_back(EntRef{entityId, entity});
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
*/


vec2 Sector::getWorldPosSectorOffset(int32_t sectorOffsetX,
                                     int32_t sectorOffsetY) const
{
    return vec2((float)(coordX - sectorOffsetX) * sectorSize,
                (float)(coordY - sectorOffsetY) * sectorSize);
}

#ifdef SERVER

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
    active = player;
}

bool Sector::removeEntity(ecs::PtrHandle* ptrHandle, ecs::EntityId entityId)
{
    /*
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
    */
    return true;
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
    ptrHandle->systems->runSystems(this, dt, ptrHandle);
    destroyMarkedEntities(ptrHandle);
}

ecs::EntityId Sector::spawnObject(ecs::PtrHandle* ptrHandle,
                                  const ecs::SpawnCallback& spwnClb)
{
    auto res = sectorRegistry.spawnObject(
        [this, ptrHandle, spwnClb](ecs::SpawnCallbackParams& clbParams)
        {
            clbParams.reg.emplace_or_replace<ecs::SectorId>(
                clbParams.entity,
                ecs::SectorId{id, (uint32_t)coordX, (uint32_t)coordY});
            if (!spwnClb || !spwnClb(clbParams))
            {
                return false;
            }
            objectInitBroadphase(ptrHandle, clbParams.entity);
            return true;
        });
    return res;
}

bool Sector::migrateObject(ecs::PtrHandle* ptrHandle, ecs::EntityId entityId)
{
    auto regMap = ptrHandle->registryMapping;
    auto slot = regMap->getEntity(entityId);
    if (!slot)
    {
        LG_W(
            "Could not migrate {} to sector. EntityId does not exist in the "
            "registry mapping.",
            entityId);
        return false;
    }
    auto lastSector = ptrHandle->world->getSector(slot->sectorId);
    if (!lastSector)
    {
        LG_W("Entities last sector does not seem to exist");
        return false;
    }
    bool res =
        sectorRegistry.migrateEntity(entityId, slot, lastSector->getRegistry());
    return res;
}

void Sector::objectInitBroadphase(ecs::PtrHandle* ptrHandle,
                                  entt::entity entity)
{
    auto reg = sectorRegistry.getRegistry();
    auto* transform = reg->try_get<ecs::Transform>(entity);
    auto* collider = reg->try_get<ecs::Collider>(entity);
    auto* broadphase = reg->try_get<ecs::Broadphase>(entity);
    if (collider && broadphase)
    {
        auto* transformCache = reg->try_get<ecs::TransformCache>(entity);
        float c = cosf(transform->rot);
        float s = sinf(transform->rot);
        if (transformCache)
        {
            transformCache->c = c;
            transformCache->s = s;
        }
        con::AABB aabb = ecs::calculateAABB(
            *transform, {c, s}, *collider, ptrHandle->colliderLib);
        if (broadphase->proxyId <= ecs::Broadphase::INVALID_PROXY_ID)
        {
            broadphase->proxyId = aabbTree.createProxy(aabb, entity);
            broadphase->fatAABB = aabb;
        }
    }
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

void Sector::markEntityForDestruction(ecs::PtrHandle* ptrHandle,
                                      ecs::EntityId entityId)
{
    auto reg = sectorRegistry.getRegistry();
    auto slot = ptrHandle->registryMapping->getEntity(entityId);
    if(!slot || slot->sectorId != id)
    {
        return;
    }
    auto& flags = reg->get<ecs::Flags>(slot->entity);
    if (flags.hasFlag(ecs::Flags::Flag::Destroyed))
    {
        return;
    }
    if (flags.hasFlag(ecs::Flags::Flag::Moved))
    {
        sectorMoveRequests.erase(
            std::remove_if(sectorMoveRequests.begin(),
                           sectorMoveRequests.end(),
                           [=](const SectorMoveRequest& request)
                           { return request.entityId == entityId; }),
            sectorMoveRequests.end());
    }
    flags.setFlag(ecs::Flags::Flag::Destroyed);
    entitiesToDestroy.push_back(entityId);
}

void Sector::destroyMarkedEntities(ecs::PtrHandle* ptrHandle)
{
    for (const auto& entityId : entitiesToDestroy)
    {
        if(!sectorRegistry.destroyObject(entityId))
        {
            LG_W("Could not destroy entityId {}", entityId.toGenericHandle32());
            continue;
        }
        ptrHandle->engine->broadcastEntityDestructionToClients(entityId);
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

void Sector::addSectorMoveRequest(ecs::PtrHandle* ptrHandle,
                                  const SectorMoveRequest& request)
{
    /*
    auto reg = ptrHandle->registry;
    auto entity = ptrHandle->ecs->getEntity(request.entityId);
    auto& flags = reg->get<ecs::Flags>(entity);

    if (flags.hasFlag((ecs::Flags::Flag)(ecs::Flags::Flag::Destroyed
                                         | ecs::Flags::Flag::Moved)))
    {
        return;
    }
    flags.setFlag(ecs::Flags::Flag::Moved);
    sectorMoveRequests.push_back(request);
    */
}

void Sector::forSectorMoveRequests(
    std::function<void(const SectorMoveRequest& request)> callback)
{
    /*
    for (const auto& request : sectorMoveRequests)
    {
        callback(request);
    }
    sectorMoveRequests.clear();
    */
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

}  // namespace world