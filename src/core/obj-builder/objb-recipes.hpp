#ifndef OBJB_RECIPES_HPP
#define OBJB_RECIPES_HPP

#include <comp-ident.hpp>
#include <comp-struct.hpp>
#include <lib-hull.hpp>
#include <lib-item.hpp>
#include <lib-modules.hpp>
#include <lib-projectile.hpp>
#include <mod-manager.hpp>
#include <ptr-handle.hpp>
#include <sector.hpp>

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
    vec2 pos = vec2(0.0f, 0.0f);
    float rot = 0.0f;
    vec2 vel = vec2(0.0f, 0.0f);
    float naturalRot = 0.0f;
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

class ItemRecipe
{
  public:
    ItemRecipe(gobj::ItemHandle itemHandle,
               ecs::EntityId collExcept = ecs::EntityId::Invalid())
        : itemHandle(itemHandle), collExcept(collExcept)
    {
    }
    ecs::EntityId spawn(const RecipeSpawnParams& params, float quantity);

  private:
    gobj::ItemHandle itemHandle;
    ecs::EntityId collExcept;
};

}  // namespace objb

#endif