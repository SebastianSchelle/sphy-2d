#include <lib-hull.hpp>

namespace gobj
{

namespace
{

constexpr const char* kHullVolumeYamlKeys[static_cast<size_t>(
    StorageType::NumStorageTypes)] = {
    "volume-container-s",
    "volume-container-l",
    "volume-tank",
    "volume-bulk",
};

}  // namespace

std::optional<vec2> colliderLocalExtents(const vector<vec2>& vertices)
{
    if (vertices.empty())
    {
        return std::nullopt;
    }
    vec2 lower = vertices.front();
    vec2 upper = vertices.front();
    for (const vec2& v : vertices)
    {
        lower.x = std::min(lower.x, v.x);
        lower.y = std::min(lower.y, v.y);
        upper.x = std::max(upper.x, v.x);
        upper.y = std::max(upper.y, v.y);
    }
    return upper - lower;
}

def::ShipClass inferShipClassFromColliderExtents(float width, float length)
{
    for (size_t i = 0; i < static_cast<size_t>(def::ShipClass::NumShipClasses); ++i)
    {
        if (width <= def::ShipClassMaxWidth[i] && length <= def::ShipClassMaxLength[i])
        {
            return static_cast<def::ShipClass>(i);
        }
    }
    return def::ShipClass::Titan;
}

def::ShipClass inferShipClassFromColliderVertices(const vector<vec2>& vertices)
{
    const std::optional<vec2> ext = colliderLocalExtents(vertices);
    if (!ext.has_value())
    {
        return def::ShipClass::Titan;
    }
    return inferShipClassFromColliderExtents(ext->x, ext->y);
}

def::ShipClass inferShipClassFromCollider(const Collider* collider)
{
    if (collider == nullptr)
    {
        return def::ShipClass::Titan;
    }
    return inferShipClassFromColliderVertices(collider->vertices);
}

float approximateHullInertiaMassFactor(float width, float length)
{
    const float w = std::max(width, 1e-6f);
    const float l = std::max(length, 1e-6f);
    return (w * w + l * l) / 12.0f;
}

float approximateHullInertia(float mass, float width, float length)
{
    const float m = std::max(mass, 1e-6f);
    return m * approximateHullInertiaMassFactor(width, length);
}

void applyColliderDerivedHullStats(Hull& hull, const Collider* collider)
{
    const std::optional<vec2> ext =
        collider != nullptr ? colliderLocalExtents(collider->vertices)
                            : std::nullopt;
    if (ext.has_value())
    {
        hull.size = ext.value();
    }
    else
    {
        hull.size = vec2(0.0f);
    }
    hull.shipClass = inferShipClassFromCollider(collider);
    hull.inertiaMassFactor =
        approximateHullInertiaMassFactor(hull.size.x, hull.size.y);
    hull.inertia = approximateHullInertia(hull.mass, hull.size.x, hull.size.y);
}

Hull Hull::fromYaml(const YAML::Node& node,
                    const con::ItemLib<gobj::Textures>& texturesLib,
                    con::ItemLib<gobj::Collider>& colliderLib)
{
    Hull hull;
    TRY_YAML_DICT(hull.name, node["name"], "");
    TRY_YAML_DICT(hull.description, node["description"], "");
    TRY_YAML_DICT(hull.hullpoints, node["hullpoints"], 100.0f);
    TRY_YAML_DICT(hull.mass, node["mass"], 1.0f);
    TRY_YAML_DICT(hull.internalGyroTorque, node["internal-gyro-torque"], 10000.0f);
    string texturesName = "";
    TRY_YAML_DICT(texturesName, node["textures"], "");
    if (texturesName != "")
    {
        hull.textures = texturesLib.getHandle(texturesName);
    }
    string colliderName = "";
    TRY_YAML_DICT(colliderName, node["collider"], "");
    if (colliderName != "")
    {
        hull.collider = colliderLib.getHandle(colliderName);
    }
    applyColliderDerivedHullStats(
        hull,
        hull.collider.isValid() ? colliderLib.getItem(hull.collider) : nullptr);
    for (size_t i = 0; i < static_cast<size_t>(StorageType::NumStorageTypes); ++i)
    {
        if (node[kHullVolumeYamlKeys[i]])
        {
            float parsed = 0.0f;
            TRY_YAML_DICT(parsed, node[kHullVolumeYamlKeys[i]], 0.0f);
            hull.volume[i] = parsed;
        }
    }
    if (node["slots"])
    {
        for (const auto& slotNode : node["slots"])
        {
            ModuleSlot slot;
            std::string modTypeStr;
            TRY_YAML_DICT(
                modTypeStr, slotNode["mod-type"], "ThrusterMainS_Common");
            slot.type =
                magic_enum::enum_cast<ModuleSlotType>(modTypeStr).value();
            TRY_YAML_DICT(slot.pos, slotNode["pos"], vec2(0.0f, 0.0f));
            float rotDeg = 0.0f;
            TRY_YAML_DICT(rotDeg, slotNode["rot"], 0.0f);
            slot.rot = smath::degToRad(rotDeg);
            if (moduleSlotTypeHasAngleLimits(slot.type))
            {
                float minAngleDeg = -180.0f;
                float maxAngleDeg = 180.0f;
                TRY_YAML_DICT(minAngleDeg, slotNode["min-angle"], minAngleDeg);
                TRY_YAML_DICT(maxAngleDeg, slotNode["max-angle"], maxAngleDeg);
                slot.minAngle = smath::degToRad(minAngleDeg);
                slot.maxAngle = smath::degToRad(maxAngleDeg);
            }
            hull.slots.push_back(slot);
        }
    }
    return hull;
}

}  // namespace gobj
