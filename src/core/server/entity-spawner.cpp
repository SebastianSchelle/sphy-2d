#include "entity-spawner.hpp"
#include "lib-station-part.hpp"
#include "std-inc.hpp"
#include "task-basic.hpp"
#include <comp-ident.hpp>
#include <components/comp-phy.hpp>

namespace sphys
{

class EntitySpawner::Spawn
{
  public:
    explicit Spawn(EntitySpawner& owner) : owner_(owner)
    {
        id_ = owner_.ecs_.createEntity();
        ent_ = owner_.ecs_.getEntity(id_);
        if (ent_ == entt::null)
        {
            failed_ = true;
        }
    }

    bool valid() const
    {
        return !failed_ && ent_ != entt::null;
    }

    entt::entity entity() const
    {
        return ent_;
    }

    ecs::EntityId id() const
    {
        return id_;
    }

    void fail()
    {
        failed_ = true;
    }

    ecs::EntityId finish()
    {
        if (failed_ || ent_ == entt::null)
        {
            return abort();
        }
        return id_;
    }

    ecs::EntityId abort()
    {
        if (ent_ != entt::null)
        {
            owner_.ecs_.destroyEntity(id_);
        }
        failed_ = true;
        return ecs::EntityId::Invalid();
    }

  private:
    EntitySpawner& owner_;
    ecs::EntityId id_;
    entt::entity ent_{entt::null};
    bool failed_{false};
};

EntitySpawner::EntitySpawner(ecs::Ecs& ecs,
                             mod::ModManager& modManager,
                             world::World& world,
                             ecs::PtrHandle* ptrHandle,
                             ai::TaskSystem& taskSystem)
    : ecs_(ecs),
      modManager_(modManager),
      world_(world),
      ptrHandle_(ptrHandle),
      taskSystem_(taskSystem)
{
}

void EntitySpawner::setItemSpawnConfig(const ItemSpawnConfig& config)
{
    itemConfig_ = config;
}

ecs::Hull* EntitySpawner::addHull(entt::entity entity,
                                const gobj::HullHandle& hullHandle)
{
    auto& reg = ecs_.getRegistry();
    auto hull = modManager_.getHullLib().getItem(hullHandle);
    if (!hull)
    {
        return nullptr;
    }
    return &reg.emplace_or_replace<ecs::Hull>(
        entity,
        ecs::Hull(hull->slots.size(),
                  hull->hullpoints,
                  hullHandle.toGenericHandle()));
}

ecs::Module* EntitySpawner::addModule(entt::entity entity,
                                      const ecs::Module& module)
{
    auto& reg = ecs_.getRegistry();
    return &reg.emplace_or_replace<ecs::Module>(entity, module);
}

ecs::Asteroid* EntitySpawner::addAsteroid(
    entt::entity entity, const gobj::AsteroidHandle& asteroidHandle)
{
    auto& reg = ecs_.getRegistry();
    auto asteroid = modManager_.getAsteroidLib().getItem(asteroidHandle);
    if (!asteroid)
    {
        return nullptr;
    }
    return &reg.emplace_or_replace<ecs::Asteroid>(
        entity,
        ecs::Asteroid{.asteroidHandle = asteroidHandle.toGenericHandle(),
                      .volume = asteroid->volume,
                      .harvestProgress = 0.0f});
}

ecs::Lifetime* EntitySpawner::addLifetime(entt::entity entity, float lifetime)
{
    auto& reg = ecs_.getRegistry();
    return &reg.emplace_or_replace<ecs::Lifetime>(
        entity, ecs::Lifetime{.lifetime = lifetime});
}

ecs::Collider* EntitySpawner::addCollider(
    entt::entity entity,
    const gobj::ColliderHandle& colliderHandle,
    ecs::CollisionLayer colliderType,
    const ecs::EntityId& exceptEntity)
{
    auto& reg = ecs_.getRegistry();
    auto collider = modManager_.getColliderLib().getItem(colliderHandle);
    if (!collider)
    {
        return nullptr;
    }
    if (!reg.all_of<ecs::Broadphase>(entity))
    {
        reg.emplace<ecs::Broadphase>(entity);
    }
    return &reg.emplace_or_replace<ecs::Collider>(
        entity,
        ecs::Collider{.colliderHandle = colliderHandle.toGenericHandle(),
                      .exceptEntity = exceptEntity,
                      .colliderType = colliderType});
}

ecs::Projectile* EntitySpawner::addProjectile(
    entt::entity entity, const gobj::ProjectileHandle& projectileHandle)
{
    auto& reg = ecs_.getRegistry();
    auto projectile = modManager_.getProjectileLib().getItem(projectileHandle);
    if (!projectile)
    {
        return nullptr;
    }
    return &reg.emplace_or_replace<ecs::Projectile>(
        entity,
        ecs::Projectile{.projectileHandle =
                            projectileHandle.toGenericHandle()});
}

ecs::MapIcon* EntitySpawner::addMapIcon(entt::entity entity)
{
    auto& reg = ecs_.getRegistry();
    auto hull = reg.try_get<ecs::Hull>(entity);
    auto station = reg.try_get<ecs::Station>(entity);

    gobj::MapIconHandle mapIconHandle = gobj::MapIconHandle::Invalid();
    if (hull)
    {
        auto hullData = modManager_.getHullLib().getItem(hull->hullHandle);
        if (hullData)
        {
            switch (hullData->shipClass)
            {
                case def::ShipClass::Spark:
                    mapIconHandle =
                        modManager_.getMapIconLib().getHandle("spark");
                    break;
                case def::ShipClass::Echo:
                    mapIconHandle =
                        modManager_.getMapIconLib().getHandle("echo");
                    break;
                default:
                    mapIconHandle =
                        modManager_.getMapIconLib().getHandle("echo");
                    break;
            }
        }
    }
    else if (station)
    {
        mapIconHandle = modManager_.getMapIconLib().getHandle("station");
    }
    if (mapIconHandle.isValid())
    {
        return &reg.emplace_or_replace<ecs::MapIcon>(
            entity,
            ecs::MapIcon{.mapIconHandle = mapIconHandle.toGenericHandle()});
    }
    LG_E("Failed to add map icon component");
    return nullptr;
}

ecs::SimpleTexture* EntitySpawner::addSimpleTexture(
    entt::entity entity, const ecs::SimpleTexture& texture)
{
    auto& reg = ecs_.getRegistry();
    return &reg.emplace_or_replace<ecs::SimpleTexture>(entity, texture);
}

ecs::Textures* EntitySpawner::addTextures(
    entt::entity entity, const gobj::TexturesHandle& texturesHandle)
{
    auto& reg = ecs_.getRegistry();
    auto textures = modManager_.getTexturesLib().getItem(texturesHandle);
    if (!textures)
    {
        return nullptr;
    }
    ecs::Textures texturesComp;
    texturesComp.texturesHandle = texturesHandle.toGenericHandle();
    return &reg.emplace_or_replace<ecs::Textures>(entity, texturesComp);
}

void EntitySpawner::addOOSSync(entt::entity entity)
{
    auto& reg = ecs_.getRegistry();
    reg.emplace<ecs::tag::OOSSync>(entity);
}

ecs::Item* EntitySpawner::addItem(entt::entity entity,
                                  const gobj::ItemHandle& itemHandle,
                                  float quantity)
{
    auto& reg = ecs_.getRegistry();
    auto item = modManager_.getItemLib().getItem(itemHandle);
    if (!item)
    {
        return nullptr;
    }
    return &reg.emplace_or_replace<ecs::Item>(
        entity,
        ecs::Item{.itemHandle = itemHandle.toGenericHandle(),
                  .quantity = quantity});
}

bool EntitySpawner::placeInSector(ecs::EntityId ent,
                                  entt::entity entity,
                                  uint32_t sectorId,
                                  const ecs::Transform& transform)
{
    auto& reg = ecs_.getRegistry();
    [[maybe_unused]] auto& tr = reg.get_or_emplace<ecs::Transform>(entity);
    ecs::TransformCache trC =
        ecs::TransformCache{cosf(transform.rot), sinf(transform.rot)};
    reg.emplace_or_replace<ecs::TransformCache>(entity, trC);
    [[maybe_unused]] auto& sec = reg.get_or_emplace<ecs::SectorId>(
        entity, ecs::SectorId{world::INVALID_SECTOR_ID, 0, 0});
    return world_.moveEntityTo(
        ptrHandle_, ent, sectorId, transform.pos, transform.rot);
}

ecs::PhysicsBody* EntitySpawner::addPhysicsBody(
    entt::entity entity, const ecs::PhysicsBody& physicsBody)
{
    auto& reg = ecs_.getRegistry();
    return &reg.emplace_or_replace<ecs::PhysicsBody>(entity, physicsBody);
}

ecs::MoveCtrl* EntitySpawner::addMoveCtrl(entt::entity entity,
                                          const ecs::PhyThrust& phyThrust,
                                          const ecs::MoveCtrl& moveCtrl)
{
    auto& reg = ecs_.getRegistry();
    reg.emplace_or_replace<ecs::PhyThrust>(entity, phyThrust);
    return &reg.emplace_or_replace<ecs::MoveCtrl>(entity, moveCtrl);
}

ecs::StationPart* EntitySpawner::addStationPart(
    entt::entity entity, const gobj::StationPartHandle& partHandle)
{
    auto& reg = ecs_.getRegistry();
    return &reg.emplace_or_replace<ecs::StationPart>(
        entity, partHandle.toGenericHandle());
}

ecs::Turret* EntitySpawner::addTurret(entt::entity entity,
                                      const ecs::Turret& turret,
                                      ai::taskdata::Turret defaultTask)
{
    auto& reg = ecs_.getRegistry();
    auto* turretComp = &reg.emplace_or_replace<ecs::Turret>(entity, turret);
    addAi(entity, defaultTask);
    return turretComp;
}

ecs::AnchorFixed* EntitySpawner::addAnchorFixed(
    entt::entity entity, const ecs::AnchorFixed& anchorFixed)
{
    auto& reg = ecs_.getRegistry();
    return &reg.emplace_or_replace<ecs::AnchorFixed>(entity, anchorFixed);
}

ecs::Storage* EntitySpawner::addStorage(entt::entity entity,
                                        const ecs::Storage& storage)
{
    auto& reg = ecs_.getRegistry();
    return &reg.emplace_or_replace<ecs::Storage>(entity, storage);
}

ecs::Ai* EntitySpawner::addAi(entt::entity entity,
                              const ai::taskdata::TaskData& defaultTask)
{
    auto& reg = ecs_.getRegistry();
    auto* sectorId = reg.try_get<ecs::SectorId>(entity);
    ai::TaskSystem* ts = &taskSystem_;
    if (sectorId && sectorId->id != world::INVALID_SECTOR_ID)
    {
        auto* sector = world_.getSector(sectorId->id);
        if (!sector)
        {
            LG_E("Sector not found");
            return nullptr;
        }
        ts = &sector->getTaskSystem();
    }

    auto& aiComp = reg.get_or_emplace<ecs::Ai>(entity);
    if (ai::TaskStackHandle(aiComp.stackHandle).isValid())
    {
        auto* taskStack = ts->getTaskStack(aiComp.stackHandle);
        if (!taskStack)
        {
            LG_W("Task stack not found for entity, creating new stack");
            aiComp.stackHandle = GenericHandle::Invalid();
        }
        else
        {
            taskStack->setDefaultTask(defaultTask);
            return &aiComp;
        }
    }
    auto stackHandle = ts->createTaskStack(defaultTask);
    aiComp.stackHandle = stackHandle.toGenericHandle();
    return &aiComp;
}

void EntitySpawner::addSelectable(entt::entity entity)
{
    auto& reg = ecs_.getRegistry();
    reg.emplace_or_replace<ecs::tag::Selectable>(entity);
}

ecs::Station* EntitySpawner::addStation(entt::entity entity)
{
    auto& reg = ecs_.getRegistry();
    return &reg.emplace_or_replace<ecs::Station>(entity);
}

bool EntitySpawner::configureShipHull(entt::entity entity,
                                      const gobj::HullHandle& hullHandle)
{
    gobj::Hull* hull = modManager_.getHullLib().getItem(hullHandle);
    if (!hull)
    {
        LG_E("Hull {} not found", hullHandle.toGenericHandle());
        return false;
    }

    addSelectable(entity);
    return addHull(entity, hullHandle)
           && addCollider(entity, hull->collider, ecs::CollisionLayer::Ship)
           && addTextures(entity, hull->textures)
           && addStorage(entity, ecs::Storage{.cargo = {}})
           && addPhysicsBody(
                  entity,
                  ecs::PhysicsBody{
                      .mass = hull->mass > 0.0f ? hull->mass : 1.0f,
                      .vel = vec2(0.0f, 0.0f),
                      .acc = vec2(0.0f, 0.0f),
                      .inertia = hull->inertia > 0.0f ? hull->inertia : 1.0f,
                      .rotVel = 0.0f,
                      .rotAcc = 0.0f})
           && addMoveCtrl(
                  entity,
                  ecs::PhyThrust{.maxTorque = 1000.0f,
                                 .maxRotVel = 3.0f,
                                 .thrustMainMax = 10000.0f,
                                 .thrustManeuverMax = 1000.0f,
                                 .maxSpd = 1000.0f},
                  ecs::MoveCtrl{.moveMode = ecs::MoveCtrl::MoveMode::None,
                                .spPos = {},
                                .allowedPosError = 100.0f,
                                .turnMode = ecs::MoveCtrl::TurnMode::None,
                                .allowedRotError = M_PIf})
           && addMapIcon(entity);
}

bool EntitySpawner::configureStationPart(
    entt::entity entity, const gobj::StationPartHandle& partHandle)
{
    gobj::StationPart* part =
        modManager_.getStationPartLib().getItem(partHandle);
    if (!part)
    {
        LG_E("Station part {} not found", partHandle.toGenericHandle());
        return false;
    }
    return addStationPart(entity, partHandle)
           && addTextures(entity, part->textures)
           && addCollider(entity, part->collider, ecs::CollisionLayer::Station);
}

ecs::EntityId EntitySpawner::spawnStation(uint32_t sectorId,
                                          const ecs::Transform& transform)
{
    Spawn spawn(*this);
    if (!spawn.valid())
    {
        LG_E("Failed to create entity for station");
        return spawn.abort();
    }

    entt::entity ent = spawn.entity();
    if (!addStation(ent) || !addMapIcon(ent))
    {
        LG_E("Failed to initialize station components");
        return spawn.abort();
    }
    addSelectable(ent);

    if (!placeInSector(spawn.id(), ent, sectorId, transform))
    {
        LG_E("Failed to place station in sector");
        return spawn.abort();
    }
    if (!addAi(ent, ai::taskdata::Idle()))
    {
        LG_E("Failed to add ai component to station");
        return spawn.abort();
    }
    addOOSSync(ent);
    return spawn.finish();
}

ecs::EntityId EntitySpawner::spawnShipHull(gobj::HullHandle hullHandle,
                                           uint32_t sectorId,
                                           const ecs::Transform& transform)
{
    Spawn spawn(*this);
    if (!spawn.valid())
    {
        LG_E("Failed to create entity for ship hull");
        return spawn.abort();
    }

    if (!configureShipHull(spawn.entity(), hullHandle))
    {
        LG_E("Failed to initialize ship hull components");
        return spawn.abort();
    }
    if (!placeInSector(spawn.id(), spawn.entity(), sectorId, transform))
    {
        LG_E("Failed to place ship hull in sector");
        return spawn.abort();
    }
    if (!addAi(spawn.entity(), ai::taskdata::Idle()))
    {
        LG_E("Failed to add ai component to ship hull");
        return spawn.abort();
    }
    addOOSSync(spawn.entity());
    return spawn.finish();
}

ecs::EntityId
EntitySpawner::addFirstStationPart(ecs::EntityId stationId,
                                   const gobj::StationPartHandle& partHandle,
                                   float rot)
{
    auto& reg = ecs_.getRegistry();
    entt::entity stationEntt = ecs_.getEntity(stationId);
    if (stationEntt == entt::null)
    {
        LG_E("Station entity not found");
        return ecs::EntityId::Invalid();
    }
    ecs::Station* station = reg.try_get<ecs::Station>(stationEntt);
    if (!station)
    {
        LG_E("Station entity has no station component");
        return ecs::EntityId::Invalid();
    }
    ecs::Transform* stationTr = reg.try_get<ecs::Transform>(stationEntt);
    if (!stationTr)
    {
        LG_E("Station entity has no transform component");
        return ecs::EntityId::Invalid();
    }
    ecs::SectorId* stationSectorId = reg.try_get<ecs::SectorId>(stationEntt);
    if (!stationSectorId)
    {
        LG_E("Station entity has no sector id component");
        return ecs::EntityId::Invalid();
    }
    gobj::StationPart* part =
        modManager_.getStationPartLib().getItem(partHandle);
    if (!part)
    {
        LG_E("Part not found");
        return ecs::EntityId::Invalid();
    }

    Spawn spawn(*this);
    if (!spawn.valid())
    {
        LG_E("Failed to create entity for station part");
        return spawn.abort();
    }
    if (!configureStationPart(spawn.entity(), partHandle))
    {
        LG_E("Failed to initialize station part components");
        return spawn.abort();
    }
    if (!placeInSector(
            spawn.id(), spawn.entity(), stationSectorId->id, {stationTr->pos, rot}))
    {
        LG_E("Failed to place station part in sector");
        return spawn.abort();
    }

    station->stationParts.push_back(
        ecs::StationPartRef{spawn.id(), part->type});
    return spawn.finish();
}

ecs::EntityId EntitySpawner::addStationPart(
    ecs::EntityId stationId,
    ecs::EntityId id1,
    const gobj::StationPartHandle& partHandle,
    uint16_t slot1,
    uint16_t slot2)
{
    auto& reg = ecs_.getRegistry();
    entt::entity stationEntt = ecs_.getEntity(stationId);
    if (stationEntt == entt::null)
    {
        LG_E("Station entity not found");
        return ecs::EntityId::Invalid();
    }
    entt::entity partEntt1 = ecs_.getEntity(id1);
    if (partEntt1 == entt::null)
    {
        LG_E("Part entity to connect to not found");
        return ecs::EntityId::Invalid();
    }
    ecs::Station* station = reg.try_get<ecs::Station>(stationEntt);
    if (!station)
    {
        LG_E("Station entity has no station component");
        return ecs::EntityId::Invalid();
    }
    ecs::SectorId* stationSectorId = reg.try_get<ecs::SectorId>(stationEntt);
    if (!stationSectorId)
    {
        LG_E("Station entity has no sector id component");
        return ecs::EntityId::Invalid();
    }
    ecs::Transform* tr1 = reg.try_get<ecs::Transform>(partEntt1);
    if (!tr1)
    {
        LG_E("Part entity to connect to has no transform component");
        return ecs::EntityId::Invalid();
    }
    ecs::StationPart* part1 = reg.try_get<ecs::StationPart>(partEntt1);
    if (!part1)
    {
        LG_E("Part entity to connect to has no station part component");
        return ecs::EntityId::Invalid();
    }
    gobj::StationPart* libPart1 =
        modManager_.getStationPartLib().getItem(part1->stationPartHandle);
    if (!libPart1)
    {
        LG_E("Library part 1 not found");
        return ecs::EntityId::Invalid();
    }
    gobj::StationPart* libPart2 =
        modManager_.getStationPartLib().getItem(partHandle);
    if (!libPart2)
    {
        LG_E("Library part 2 not found");
        return ecs::EntityId::Invalid();
    }

    if (slot1 >= libPart1->connectors.size())
    {
        LG_E("Slot index 1 out of range");
        return ecs::EntityId::Invalid();
    }
    gobj::Connector& connector1 = libPart1->connectors[slot1];
    if (slot2 >= libPart2->connectors.size())
    {
        LG_E("Slot index 2 out of range");
        return ecs::EntityId::Invalid();
    }
    gobj::Connector& connector2 = libPart2->connectors[slot2];
    vec2 offset1 = smath::rotateVec2(connector1.pos, tr1->rot);

    float rot2 = M_PIf + tr1->rot + connector1.rot - connector2.rot;
    vec2 offset2 = smath::rotateVec2(connector2.pos, rot2);
    vec2 part2Pos = tr1->pos + offset1 - offset2;

    Spawn spawn(*this);
    if (!spawn.valid())
    {
        LG_E("Failed to create entity for station part");
        return spawn.abort();
    }
    if (!configureStationPart(spawn.entity(), partHandle))
    {
        LG_E("Failed to initialize station part components");
        return spawn.abort();
    }
    if (!placeInSector(
            spawn.id(), spawn.entity(), stationSectorId->id, {part2Pos, rot2}))
    {
        LG_E("Failed to place station part in sector");
        return spawn.abort();
    }

    station->stationParts.push_back(
        ecs::StationPartRef{spawn.id(), libPart2->type});
    return spawn.finish();
}

ecs::EntityId EntitySpawner::spawnModule(ecs::EntityId parent,
                                         const gobj::ModuleHandle& moduleHandle,
                                         uint16_t slotIndex)
{
    auto& reg = ecs_.getRegistry();
    entt::entity parentEntt = ecs_.getEntity(parent);
    if (parentEntt == entt::null)
    {
        LG_E("Parent entity not found for module");
        return ecs::EntityId::Invalid();
    }
    ecs::SectorId* sectorId = reg.try_get<ecs::SectorId>(parentEntt);
    if (!sectorId)
    {
        LG_E("Parent entity has no sector id component");
        return ecs::EntityId::Invalid();
    }
    gobj::Module* module = modManager_.getModuleLib().getItem(moduleHandle);
    if (!module)
    {
        LG_E("Module with handle {} not found", moduleHandle.toGenericHandle());
        return ecs::EntityId::Invalid();
    }
    auto hull = reg.try_get<ecs::Hull>(parentEntt);
    if (!hull)
    {
        LG_E("Parent entity has no hull component");
        return ecs::EntityId::Invalid();
    }
    gobj::Hull* hullItem = modManager_.getHullLib().getItem(hull->hullHandle);
    if (!hullItem)
    {
        LG_E("Hull item not found");
        return ecs::EntityId::Invalid();
    }
    if (slotIndex >= hullItem->slots.size())
    {
        LG_E("Slot index out of range");
        return ecs::EntityId::Invalid();
    }
    gobj::ModuleSlot& slot = hullItem->slots[slotIndex];
    if (slot.type != module->slotType)
    {
        LG_E("Slot type mismatch");
        return ecs::EntityId::Invalid();
    }

    Spawn spawn(*this);
    if (!spawn.valid())
    {
        LG_E("Failed to create entity for module");
        return spawn.abort();
    }

    entt::entity ent = spawn.entity();
    addTextures(ent, module->textures);
    placeInSector(spawn.id(), ent, sectorId->id, {{0.0f, 0.0f}, 0.0f});
    addAnchorFixed(
        ent,
        ecs::AnchorFixed{.pos = slot.pos, .rot = slot.rot, .ref = parent});
    addModule(ent,
              ecs::Module{moduleHandle.toGenericHandle(),
                          parent.toGenericHandle32()});

    switch (module->type)
    {
        case gobj::ModuleType::MainThruster:
            break;
        case gobj::ModuleType::ManeuverThruster:
            break;
        case gobj::ModuleType::Storage:
            break;
        case gobj::ModuleType::Turret:
        {
            gobj::mdata::Turret libTurretData =
                std::get<gobj::mdata::Turret>(module->data);
            if (!addTurret(
                    ent,
                    ecs::Turret{
                        .aimMode = ecs::Turret::AimMode::None,
                        .aimData = ecs::Turret::AngleData{0.0f},
                        .data = ecs::Turret::fromGobjTurretData(libTurretData),
                        .currentAngle = 0.0f,
                        .isFiring = false,
                    },
                    ai::taskdata::Turret{ai::taskdata::Turret::Mode::Mine,
                                         ai::taskdata::Turret::ConfigMine{}}))
            {
                LG_E("Failed to add turret component");
            }
            break;
        }
        default:
            break;
    }

    hull->addModule(slotIndex,
                    ecs::ModuleRef{spawn.id(), module->type, module->slotType});
    return spawn.finish();
}

ecs::EntityId EntitySpawner::spawnProjectile(
    uint32_t sectorId,
    vec2 pos,
    vec2 vel,
    gobj::ProjectileHandle projectileHandle,
    const ecs::EntityId& exceptEntity,
    vec2 parVel)
{
    auto projectile = modManager_.getProjectileLib().getItem(projectileHandle);
    if (!projectile)
    {
        LG_E("Projectile {} not found", projectileHandle.toGenericHandle());
        return ecs::EntityId::Invalid();
    }

    Spawn spawn(*this);
    if (!spawn.valid())
    {
        LG_E("Failed to create entity for projectile");
        return spawn.abort();
    }

    entt::entity ent = spawn.entity();
    addSelectable(ent);
    addProjectile(ent, projectileHandle);
    addLifetime(ent, projectile->lifetime);
    addTextures(ent, projectile->textures);
    addCollider(ent,
                projectile->collider,
                ecs::CollisionLayer::Projectile,
                exceptEntity);
    addPhysicsBody(ent,
                   ecs::PhysicsBody{.mass = 1.0f,
                                    .vel = vel + parVel,
                                    .acc = vec2(0.0f, 0.0f),
                                    .inertia = 1.0f,
                                    .rotVel = 0.0f,
                                    .rotAcc = 0.0f});

    float rot = atan2f(-vel.x, vel.y);
    if (!placeInSector(spawn.id(), ent, sectorId, {pos, rot}))
    {
        LG_E("Failed to place projectile in sector");
        return spawn.abort();
    }
    return spawn.finish();
}

ecs::EntityId EntitySpawner::spawnAsteroid(uint32_t sectorId,
                                         const ecs::Transform& transform,
                                         const gobj::AsteroidHandle& asteroidHandle,
                                         float rotVel)
{
    auto asteroid = modManager_.getAsteroidLib().getItem(asteroidHandle);
    if (!asteroid)
    {
        LG_E("Asteroid {} not found", asteroidHandle.toGenericHandle());
        return ecs::EntityId::Invalid();
    }

    Spawn spawn(*this);
    if (!spawn.valid())
    {
        LG_E("Failed to create entity for asteroid");
        return spawn.abort();
    }

    entt::entity ent = spawn.entity();
    addAsteroid(ent, asteroidHandle);
    addSelectable(ent);
    addTextures(ent, asteroid->textures);
    addCollider(ent, asteroid->collider, ecs::CollisionLayer::Asteroid);

    auto* colliderData =
        modManager_.getColliderLib().getItem(asteroid->collider);
    vec2 extends = vec2(0.5f, 0.5f);
    if (colliderData)
    {
        extends = smath::colliderLocalExtents(colliderData->vertices);
    }
    else
    {
        LG_E("Collider data for {} not found",
             asteroid->collider.toGenericHandle());
    }
    float inertia =
        smath::approximateInertia(asteroid->volume, extends.x, extends.y);
    addPhysicsBody(ent,
                   ecs::PhysicsBody{.mass = asteroid->volume * 3000.0f,
                                    .vel = vec2(0.0f, 0.0f),
                                    .acc = vec2(0.0f, 0.0f),
                                    .inertia = inertia,
                                    .rotVel = rotVel,
                                    .rotAcc = 0.0f,
                                    .naturalRotation = rotVel});

    if (!placeInSector(spawn.id(), ent, sectorId, transform))
    {
        LG_E("Failed to place asteroid in sector");
        return spawn.abort();
    }
    return spawn.finish();
}

ecs::EntityId EntitySpawner::spawnItem(uint32_t sectorId,
                                       const ecs::Transform& transform,
                                       const gobj::ItemHandle& itemHandle,
                                       float quantity,
                                       vec2 initialVel,
                                       ecs::EntityId collexcept)
{
    auto item = modManager_.getItemLib().getItem(itemHandle);
    if (!item)
    {
        LG_E("Item {} not found", itemHandle.toGenericHandle());
        return ecs::EntityId::Invalid();
    }

    Spawn spawn(*this);
    if (!spawn.valid())
    {
        LG_E("Failed to create entity for item");
        return spawn.abort();
    }

    entt::entity ent = spawn.entity();
    addItem(ent, itemHandle, quantity);
    addSelectable(ent);
    addSimpleTexture(
        ent, ecs::SimpleTexture{.textureHandle = item->worldTexture});
    addCollider(ent,
                itemConfig_.colliderHandle,
                ecs::CollisionLayer::Item,
                collexcept);
    addPhysicsBody(ent,
                   ecs::PhysicsBody{.mass = 100.0f,
                                    .vel = initialVel,
                                    .acc = vec2(0.0f, 0.0f),
                                    .inertia = 1.0f,
                                    .rotVel = 0.0f,
                                    .rotAcc = 0.0f});
    addLifetime(ent, itemConfig_.lifetime);

    if (!placeInSector(spawn.id(), ent, sectorId, transform))
    {
        LG_E("Failed to place item in sector");
        return spawn.abort();
    }
    return spawn.finish();
}

}  // namespace sphys
