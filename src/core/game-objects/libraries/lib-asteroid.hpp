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

/**
 * Parent asteroids spawn child fragment asteroids when destroyed.
 * Fragment asteroids are mined directly for items.
 */
enum class AsteroidType : uint8_t
{
    Parent,
    Fragment,
};

/** Child asteroid handle and how many instances spawn when parent breaks apart. */
using AsteroidChildSpawn = std::pair<AsteroidHandle, uint8_t>;
using AsteroidChildren = std::vector<AsteroidChildSpawn>;
struct AsteroidParentdata
{
    AsteroidChildren children;
};

using AsteroidComposition = std::vector<std::pair<ItemHandle, float>>;
struct AsteroidFragmentdata
{
    AsteroidComposition composition;
};

using AsteroidContent = std::variant<AsteroidParentdata, AsteroidFragmentdata>;

struct Asteroid
{
    string name;
    string description;
    TexturesHandle textures = TexturesHandle::Invalid();
    ColliderHandle collider = ColliderHandle::Invalid();
    AsteroidType type = AsteroidType::Fragment;
    AsteroidContent content;
    /** (4/3)πr³ with r = avg(collider width, height) from collider extents at load. */
    float volume = 0.0f;

    static Asteroid fromYaml(const YAML::Node& node,
                             const con::ItemLib<gobj::Item>& itemLib,
                             const con::ItemLib<gobj::Textures>& texturesLib,
                             con::ItemLib<gobj::Collider>& colliderLib);

    /** Resolve parent child handles after all asteroids in a load batch are registered. */
    static void loadChildrenFromYaml(
        Asteroid& asteroid,
        const YAML::Node& node,
        const con::ItemLib<gobj::Asteroid>& asteroidLib);
};

}  // namespace gobj

EXT_FMT(gobj::AsteroidType, "{}", magic_enum::enum_name(o));
EXT_FMT(gobj::Asteroid,
        "(name: {}, type: {}, textures: {}, collider: {}, volume: {})",
        o.name,
        o.type,
        o.textures.toString(),
        o.collider.toString(),
        o.volume);

#endif
