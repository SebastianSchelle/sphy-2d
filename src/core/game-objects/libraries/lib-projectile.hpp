#ifndef LIB_PROJECTILE_HPP
#define LIB_PROJECTILE_HPP

#include <item-lib.hpp>
#include <std-inc.hpp>
#include <turret-def.hpp>
#include <lib-textures.hpp>
#include <lib-collider.hpp>

namespace gobj
{

struct Projectile
{
    string name;
    string description;
    float dmg = 1.0f;
    float lifetime = 10.0f;
    def::DamageType damageType = def::DamageType::Kinetic;
    TexturesHandle textures = TexturesHandle::Invalid();
    ColliderHandle collider = ColliderHandle::Invalid();

    static Projectile fromYaml(const YAML::Node& node,
                               const con::ItemLib<gobj::Textures>& texturesLib,
                               const con::ItemLib<gobj::Collider>& colliderLib);
};

struct Missile
{
    string name;
    string description;
    float dmg = 1000.0f;
    float detonationRadius = 20.0f;
    float lifetime = 10.0f;
    def::DamageType damageType = def::DamageType::Explosive;
    TexturesHandle textures = TexturesHandle::Invalid();
    ColliderHandle collider = ColliderHandle::Invalid();

    static Missile fromYaml(const YAML::Node& node,
                            const con::ItemLib<gobj::Textures>& texturesLib,
                            const con::ItemLib<gobj::Collider>& colliderLib);
};

using ProjectileHandle = typename con::ItemLib<Projectile>::Handle;
using MissileHandle = typename con::ItemLib<Missile>::Handle;

}  // namespace gobj

EXT_FMT(gobj::Projectile,
        "(name: {}, description: {}, dmg: {}, lifetime: {}, damageType: {}, "
        "textures: {}, collider: {})",
        o.name,
        o.description,
        o.dmg,
        o.lifetime,
        o.damageType,
        o.textures.toString(),
        o.collider.toString());

EXT_FMT(gobj::Missile,
        "(name: {}, description: {}, dmg: {}, detonationRadius: {}, lifetime: "
        "{}, damageType: {}, textures: {}, collider: {})",
        o.name,
        o.description,
        o.dmg,
        o.detonationRadius,
        o.lifetime,
        o.damageType,
        o.textures.toString(),
        o.collider.toString());

#endif
