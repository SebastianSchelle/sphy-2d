#ifndef LIB_HULL_HPP
#define LIB_HULL_HPP

#include <item-lib.hpp>
#include <lib-collider.hpp>
#include <lib-modules.hpp>
#include <lib-textures.hpp>
#include <magic_enum/magic_enum.hpp>
#include <std-inc.hpp>
#include <ship-def.hpp>

namespace gobj
{

struct Hull
{
    string name;
    string description;
    float hullpoints;
    /** Collider AABB width (x) and length (y); not authored in YAML. */
    vec2 size;
    float mass;
    /** I = mass * inertiaMassFactor; uniform rectangle approximation. */
    float inertia = 1.0f;
    /** (width² + length²) / 12 */
    float inertiaMassFactor = 1.0f;
    def::ShipClass shipClass;
    vector<ModuleSlot> slots;
    TexturesHandle textures = TexturesHandle::Invalid();
    ColliderHandle collider = ColliderHandle::Invalid();
    float volume[static_cast<size_t>(gobj::ItemStorageType::NumStorageTypes)] = {
        0.0f};
    /** Built-in attitude torque limit (N·m); `internal-gyro-torque` in YAML. */
    float internalGyroTorque = 10000.0f;

    static Hull fromYaml(const YAML::Node& node,
                         const con::ItemLib<gobj::Textures>& texturesLib,
                         con::ItemLib<gobj::Collider>& colliderLib);
};

using HullHandle = typename con::ItemLib<Hull>::Handle;

/** Smallest ship class whose max width/length fit the given extents. */
def::ShipClass inferShipClassFromColliderExtents(float width, float length);

def::ShipClass inferShipClassFromColliderVertices(const vector<vec2>& vertices);

def::ShipClass inferShipClassFromCollider(const Collider* collider);

void applyColliderDerivedHullStats(Hull& hull, const Collider* collider);

}  // namespace gobj

EXT_FMT(gobj::Hull,
        "(hullpoints: {}, name: {}, textures: {}, collider: {}, size: {}, "
        "mass: {}, inertia: {}, shipClass: {}, description: {}, slots: {})",
        o.hullpoints,
        o.name,
        o.textures.toString(),
        o.collider.toString(),
        o.size,
        o.mass,
        o.inertia,
        o.shipClass,
        o.description,
        o.slots);

#endif  // LIB_HULL_HPP