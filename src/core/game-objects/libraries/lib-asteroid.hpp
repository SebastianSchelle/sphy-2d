#ifndef LIB_ASTEROID_HPP
#define LIB_ASTEROID_HPP

#include <std-inc.hpp>
#include <lib-textures.hpp>
#include <lib-collider.hpp>
#include <lib-item.hpp>
#include <magic_enum/magic_enum.hpp>

namespace gobj
{

struct Asteroid;

/** Declared before Asteroid is complete; Handle does not depend on sizeof(Asteroid). */
using AsteroidHandle = typename con::ItemLib<Asteroid>::Handle;

struct Asteroid
{
    string name;
    string description;
    TexturesHandle textures = TexturesHandle::Invalid();
    ColliderHandle collider = ColliderHandle::Invalid();
    float mass;
    float maxHp;

    std::vector<std::pair<ItemHandle, float>> composition;
    std::vector<std::pair<AsteroidHandle, uint8_t>> debris;

    static Asteroid fromYaml(const YAML::Node& node,
                             const con::ItemLib<gobj::Item>& itemLib,
                             const con::ItemLib<gobj::Textures>& texturesLib,
                             const con::ItemLib<gobj::Collider>& colliderLib,
                             const con::ItemLib<gobj::Asteroid>& asteroidLib);

    /** Resolve debris handles after all asteroids in a load batch are registered. */
    static void loadDebrisFromYaml(
        Asteroid& asteroid,
        const YAML::Node& node,
        const con::ItemLib<gobj::Asteroid>& asteroidLib);
};

}  // namespace gobj

EXT_FMT(gobj::Asteroid,
        "(name: {}, description: {}, textures: {}, collider: {}, mass: {}, "
        "maxHp: {}, composition: {}, debris: {})",
        o.name,
        o.description,
        o.textures.toString(),
        o.collider.toString(),
        o.mass,
        o.maxHp,
        o.composition.size(),
        o.debris.size());

#endif