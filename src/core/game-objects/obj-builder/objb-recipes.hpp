#ifndef OBJB_RECIPES_HPP
#define OBJB_RECIPES_HPP

#include "comp-ident.hpp"
#include "lib-hull.hpp"
#include "lib-modules.hpp"
#include "sector.hpp"
#include <ptr-handle.hpp>

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

}  // namespace objb

#endif