#ifndef LIB_COLLIDER_HPP
#define LIB_COLLIDER_HPP

#include <std-inc.hpp>
#include <item-lib.hpp>
#include <lib-textures.hpp>
#include <magic_enum/magic_enum.hpp>

namespace gobj
{

struct Collider
{
    vector<vec2> vertices;
    float restitution = 0.5f;

    static Collider fromYaml(const YAML::Node& node,
                             mod::ResourceMap& resourceMap);
};

using ColliderHandle = typename con::ItemLib<Collider>::Handle;

}  // namespace gobj

EXT_FMT(gobj::Collider,
        "(vertices: {}, restitution: {})",
        o.vertices,
        o.restitution);

#endif  // LIB_COLLIDER_HPP