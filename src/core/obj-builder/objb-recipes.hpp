#ifndef OBJB_RECIPES_HPP
#define OBJB_RECIPES_HPP

#include <comp-ident.hpp>
#include <comp-struct.hpp>
#include <lib-hull.hpp>
#include <lib-item.hpp>
#include <lib-modules.hpp>
#include <lib-projectile.hpp>
#include <lib-recipes.hpp>
#include <ptr-handle.hpp>
#include <sector.hpp>

namespace objb
{

struct RecipeSpawnParams
{
    ecs::PtrHandle* ptrHandle;
    world::Sector* sector;
    vec2 pos = vec2(0.0f, 0.0f);
    float rot = 0.0f;
    vec2 vel = vec2(0.0f, 0.0f);
    float naturalRot = 0.0f;
};

namespace ShipRecipe
{
  ecs::EntityId spawn(const gobj::ShipRecipeHandle handle, const RecipeSpawnParams& params);
}

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

}  // namespace objb

#endif