#ifndef LIB_HULL_HPP
#define LIB_HULL_HPP

#include <item-lib.hpp>
#include <lib-collider.hpp>
#include <lib-modules.hpp>
#include <lib-textures.hpp>
#include <magic_enum/magic_enum.hpp>
#include <std-inc.hpp>

namespace gobj
{

struct Hull
{
    string name;
    string description;
    float hullpoints;
    vector<ModuleSlot> slots;
    TexturesHandle textures = TexturesHandle::Invalid();
    ColliderHandle collider = ColliderHandle::Invalid();
    MapIconHandle mapIcon = MapIconHandle::Invalid();

    static Hull fromYaml(const YAML::Node& node,
                         const con::ItemLib<gobj::Textures>& texturesLib,
                         const con::ItemLib<gobj::Collider>& colliderLib,
                         const con::ItemLib<gobj::MapIcon>& mapIconLib);
};

using HullHandle = typename con::ItemLib<Hull>::Handle;


}  // namespace gobj

EXT_FMT(gobj::Hull,
        "(hullpoints: {}, name: {}, textures: {}, collider: {}, mapIcon: {}, "
        "description: {}, slots: {})",
        o.hullpoints,
        o.name,
        o.textures.toString(),
        o.collider.toString(),
        o.mapIcon.toString(),
        o.description,
        o.slots);

#endif  // LIB_HULL_HPP