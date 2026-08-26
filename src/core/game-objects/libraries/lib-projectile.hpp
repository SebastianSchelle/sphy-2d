#ifndef LIB_PROJECTILE_HPP
#define LIB_PROJECTILE_HPP

#include <item-lib.hpp>
#include <lib-collider.hpp>
#include <lib-textures.hpp>
#include <std-inc.hpp>
#include <turret-def.hpp>

namespace gobj
{

struct Projectile
{
    string name;
    string description;
    float dmg = 1.0f;
    float lifetime = 1.0f;
    def::DamageType damageType = def::DamageType::Kinetic;
    // todo: remove collider and change textures to texture, unnecessary
    // overkill
    TexturesHandle textures = TexturesHandle::Invalid();

    static Projectile fromYaml(const YAML::Node& node,
                               const con::ItemLib<gobj::Textures>& texturesLib);
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

    static Missile fromYaml(const YAML::Node& node,
                            const con::ItemLib<gobj::Textures>& texturesLib);
};

struct Beam
{
    string name;
    string description;
    float dps = 10.0f;
    float range = 10.0f;
    float lifetime = 1.0f;
    def::DamageType damageType = def::DamageType::Energy;

    static Beam fromYaml(const YAML::Node& node);
};

using ProjectileHandle = typename con::ItemLib<Projectile>::Handle;
using MissileHandle = typename con::ItemLib<Missile>::Handle;
using BeamHandle = typename con::ItemLib<Beam>::Handle;


}  // namespace gobj

EXT_FMT(gobj::Projectile,
        "(name: {}, description: {}, dmg: {}, lifetime: {}, damageType: {}, "
        "textures: {})",
        o.name,
        o.description,
        o.dmg,
        o.lifetime,
        o.damageType,
        o.textures.toString());

EXT_FMT(gobj::Missile,
        "(name: {}, description: {}, dmg: {}, detonationRadius: {}, lifetime: "
        "{}, damageType: {}, textures: {})",
        o.name,
        o.description,
        o.dmg,
        o.detonationRadius,
        o.lifetime,
        o.damageType,
        o.textures.toString());

EXT_FMT(gobj::Beam,
        "(name: {}, description: {}, dps: {}, lifetime: "
        "{}, damageType: {})",
        o.name,
        o.description,
        o.dps,
        o.lifetime,
        o.damageType);

#endif
