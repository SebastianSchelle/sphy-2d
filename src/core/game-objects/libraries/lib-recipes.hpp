#ifndef LIB_RECIPES_HPP
#define LIB_RECIPES_HPP


#include "lib-hull.hpp"
#include "lib-modules.hpp"

namespace gobj
{


struct RecipeModuleSlot
{
    uint32_t slot;
    gobj::ModuleHandle modHandle;
};

struct ShipRecipe
{
    string name;
    string description;
    gobj::HullHandle hullHandle;
    std::vector<RecipeModuleSlot> modSlot;

    static ShipRecipe fromYaml(const YAML::Node& node,
                               con::ItemLib<gobj::Hull>& hullLib,
                               con::ItemLib<gobj::Module>& moduleLib);
};

using ShipRecipeHandle = typename con::ItemLib<ShipRecipe>::Handle;

}  // namespace gobj

#endif