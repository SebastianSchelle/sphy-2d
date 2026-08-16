#ifndef OBJB_RECIPES_HPP
#define OBJB_RECIPES_HPP

#include "comp-ident.hpp"
#include "comp-struct.hpp"
#include "lib-hull.hpp"
#include "lib-modules.hpp"
#include "lib-projectile.hpp"
#include "sector.hpp"
#include <ptr-handle.hpp>
#include <mod-manager.hpp>

namespace objb
{

struct ModuleSlot
{
    uint32_t slot;
    gobj::ModuleHandle modHandle;
};

struct RecipeSpawnParams
{
    ecs::PtrHandle* ptrHandle;
    world::Sector* sector;
    vec2 pos;
    float rot;
    float naturalRot;
};

class ShipRecipe
{
  public:
    ShipRecipe(gobj::HullHandle hullHandle,
               const std::vector<ModuleSlot> modSlots)
        : hullHandle(hullHandle), modSlot(modSlots)
    {
    }
    ecs::EntityId spawn(const RecipeSpawnParams& params);

  private:
    gobj::HullHandle hullHandle;
    std::vector<ModuleSlot> modSlot;
};

class AsteroidRecipe
{
  public:
    AsteroidRecipe(gobj::AsteroidHandle asteroidHandle)
        : asteroidHandle(asteroidHandle)
    {
    }
    ecs::EntityId spawn(const RecipeSpawnParams& params);

  private:
    gobj::AsteroidHandle asteroidHandle;
};

class ProjectileRecipe
{
  public:
    ProjectileRecipe(gobj::ProjectileHandle projectileHandle,
                     ecs::EntityId exceptEntity, float exitSpeed)
        : projectileHandle(projectileHandle), exceptEntity(exceptEntity), exitSpeed(exitSpeed)
    {
    }
    ecs::EntityId spawn(const RecipeSpawnParams& params, float s, float c, vec2 parentvel);

  private:
    gobj::ProjectileHandle projectileHandle;
    ecs::EntityId exceptEntity;
    float exitSpeed;
};

}  // namespace objb

#endif