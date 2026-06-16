#ifndef ENTITY_SPAWNER_HPP
#define ENTITY_SPAWNER_HPP

#include "ecs.hpp"
#include "world-def.hpp"
#include <comp-ai.hpp>
#include <comp-gfx.hpp>
#include <comp-lifetime.hpp>
#include <comp-phy.hpp>
#include <comp-storage.hpp>
#include <comp-struct.hpp>
#include <comp-tag.hpp>
#include <comp-turret.hpp>
#include <lib-hull.hpp>
#include <mod-manager.hpp>
#include <task-system.hpp>
#include <world.hpp>

namespace sphys
{

class EntitySpawner
{
  public:
    struct ItemSpawnConfig
    {
        gobj::ColliderHandle colliderHandle = gobj::ColliderHandle::Invalid();
        float lifetime = 600.0f;
    };

    EntitySpawner(ecs::Ecs& ecs,
                  mod::ModManager& modManager,
                  world::World& world,
                  ecs::PtrHandle* ptrHandle,
                  ai::TaskSystem& taskSystem);

    void setItemSpawnConfig(const ItemSpawnConfig& config);

    ecs::EntityId spawnStation(uint32_t sectorId, const ecs::Transform& transform);
    ecs::EntityId spawnShipHull(gobj::HullHandle hullHandle,
                                uint32_t sectorId,
                                const ecs::Transform& transform);
    ecs::EntityId addFirstStationPart(ecs::EntityId stationId,
                                      const gobj::StationPartHandle& partHandle,
                                      float rot);
    ecs::EntityId addStationPart(ecs::EntityId stationId,
                                 ecs::EntityId partIdConnectTo,
                                 const gobj::StationPartHandle& partHandle,
                                 uint16_t slotConnectTo,
                                 uint16_t slotNewPart);
    ecs::EntityId spawnModule(ecs::EntityId parent,
                              const gobj::ModuleHandle& moduleHandle,
                              uint16_t slotIndex);
    ecs::EntityId spawnProjectile(uint32_t sectorId,
                                  vec2 pos,
                                  vec2 vel,
                                  gobj::ProjectileHandle projectileHandle,
                                  const ecs::EntityId& exceptEntity,
                                  vec2 parVel);
    ecs::EntityId spawnAsteroid(uint32_t sectorId,
                                const ecs::Transform& transform,
                                const gobj::AsteroidHandle& asteroidHandle,
                                float rotVel);
    ecs::EntityId spawnItem(uint32_t sectorId,
                            const ecs::Transform& transform,
                            const gobj::ItemHandle& itemHandle,
                            float quantity,
                            vec2 initialVel,
                            ecs::EntityId collexcept);

  private:
    class Spawn;

    bool placeInSector(ecs::EntityId ent,
                       entt::entity entity,
                       uint32_t sectorId,
                       const ecs::Transform& transform);

    bool configureShipHull(entt::entity entity,
                           const gobj::HullHandle& hullHandle);
    bool configureStationPart(entt::entity entity,
                              const gobj::StationPartHandle& partHandle);

    ecs::Hull* addHull(entt::entity entity, const gobj::HullHandle& hullHandle);
    ecs::Station* addStation(entt::entity entity);
    ecs::Collider* addCollider(entt::entity entity,
                               const gobj::ColliderHandle& colliderHandle,
                               ecs::CollisionLayer colliderType,
                               const ecs::EntityId& exceptEntity =
                                   ecs::EntityId::Invalid());
    ecs::Projectile* addProjectile(entt::entity entity,
                                   const gobj::ProjectileHandle& projectileHandle);
    ecs::MapIcon* addMapIcon(entt::entity entity);
    ecs::SimpleTexture* addSimpleTexture(entt::entity entity,
                                         const ecs::SimpleTexture& texture);
    ecs::Textures* addTextures(entt::entity entity,
                               const gobj::TexturesHandle& texturesHandle);
    ecs::PhysicsBody* addPhysicsBody(entt::entity entity,
                                     const ecs::PhysicsBody& physicsBody);
    ecs::MoveCtrl* addMoveCtrl(entt::entity entity,
                               const ecs::PhyThrust& phyThrust,
                               const ecs::MoveCtrl& moveCtrl);
    ecs::Asteroid* addAsteroid(entt::entity entity,
                               const gobj::AsteroidHandle& asteroidHandle);
    ecs::Module* addModule(entt::entity entity, const ecs::Module& module);
    ecs::Item* addItem(entt::entity entity,
                       const gobj::ItemHandle& itemHandle,
                       float quantity);
    ecs::Storage* addStorage(entt::entity entity, const ecs::Storage& storage);
    ecs::Lifetime* addLifetime(entt::entity entity, float lifetime);
    void addOOSSync(entt::entity entity);
    ecs::StationPart* addStationPart(entt::entity entity,
                                     const gobj::StationPartHandle& partHandle);
    ecs::Turret* addTurret(entt::entity entity,
                           const ecs::Turret& turret,
                           ai::taskdata::Turret defaultTask);
    void addSelectable(entt::entity entity);
    ecs::AnchorFixed* addAnchorFixed(entt::entity entity,
                                     const ecs::AnchorFixed& anchorFixed);
    ecs::Ai* addAi(entt::entity entity,
                   const ai::taskdata::TaskData& defaultTask =
                       ai::taskdata::Idle());

    ecs::Ecs& ecs_;
    mod::ModManager& modManager_;
    world::World& world_;
    ecs::PtrHandle* ptrHandle_;
    ai::TaskSystem& taskSystem_;
    ItemSpawnConfig itemConfig_;
};

}  // namespace sphys

#endif
