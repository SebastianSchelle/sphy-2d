#ifndef COMP_PHY_HPP
#define COMP_PHY_HPP

#include <entt/entt.hpp>
#include <std-inc.hpp>
#include <yaml-cpp/yaml.h>
#include <magic_enum/magic_enum.hpp>
#include <world-def.hpp>

namespace ecs
{

struct Transform
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "transform";

    vec2 pos;
    float rot;

    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node)
    {
        Transform transform;
        transform.pos = node["pos"].as<vec2>();
        transform.rot = node["rot"].as<float>();
        registry.emplace<Transform>(entity, transform);
    }
};

#define SER_TRANSFORM                                                          \
    SOBJ(o.pos);                                                               \
    S4b(o.rot);

EXT_SER(Transform, SER_TRANSFORM)
EXT_DES(Transform, SER_TRANSFORM)

struct TransformNet
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "trans-net";

    TransformNet(Transform transform)
    {
        this->transform = transform;
    }
    Transform transform;
    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node)
    {
        Transform transform;
        transform.pos = node["pos"].as<vec2>();
        transform.rot = node["rot"].as<float>();
        TransformNet transformNet(transform);
        registry.emplace<TransformNet>(entity, transformNet);
    }
};

#define SER_TRANSFORM_NET SOBJ(o.transform);
EXT_SER(TransformNet, SER_TRANSFORM_NET)
EXT_DES(TransformNet, SER_TRANSFORM_NET)

struct PhysicsBody
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "physics-body";

    float mass = 1.0f;        // kg
    vec2 vel = {0.0f, 0.0f};  // m/s
    vec2 acc = {0.0f, 0.0f};  // m/s^2
    float inertia = 1.0f;     // kg*m^2
    float rotVel = 0.0f;      // rad/s
    float rotAcc = 0.0f;      // rad/s^2

    void applyForce(vec2 force)
    {
        acc += force / mass;
    }
    void applyTorque(float torque)
    {
        rotAcc += torque / inertia;
    }
    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node)
    {
        PhysicsBody physicsBody;
        TRY_YAML_DICT(physicsBody.mass, node["mass"], 1.0f);
        TRY_YAML_DICT(physicsBody.inertia, node["inertia"], 1.0f);
        TRY_YAML_DICT(physicsBody.vel, node["vel"], vec2(0.0f, 0.0f));
        TRY_YAML_DICT(physicsBody.rotVel, node["rotVel"], 0.0f);
        TRY_YAML_DICT(physicsBody.rotAcc, node["rotAcc"], 0.0f);
        TRY_YAML_DICT(physicsBody.acc, node["acc"], vec2(0.0f, 0.0f));
        if (physicsBody.inertia <= 1e-8f)
        {
            physicsBody.inertia = 1.0f;
        }
        if (physicsBody.mass <= 1e-8f)
        {
            physicsBody.mass = 1.0f;
        }
        registry.emplace<PhysicsBody>(entity, physicsBody);
    }
};

#define SER_PHYSICS_BODY                                                       \
    S4b(o.mass);                                                               \
    S4b(o.inertia);                                                            \
    SOBJ(o.vel);                                                               \
    S4b(o.rotVel);                                                             \
    S4b(o.rotAcc);                                                             \
    SOBJ(o.acc);
EXT_SER(PhysicsBody, SER_PHYSICS_BODY)
EXT_DES(PhysicsBody, SER_PHYSICS_BODY)

/// Scale local thrust uniformly so |x|<=maneuverMax, |y|<=mainMax; preserves
/// direction (per-axis clamp does not). Hot path: already inside box (no div).
inline void
clampThrustLocalToActuatorBox(vec2& local, float maneuverMax, float mainMax)
{
    constexpr float eps = 1e-12f;
    const float tx = local.x;
    const float ty = local.y;
    const float ax = std::fabsf(tx);
    const float ay = std::fabsf(ty);
    if (ax < eps && ay < eps)
    {
        local = vec2(0.0f);
        return;
    }
    if (ax <= maneuverMax && ay <= mainMax)
    {
        return;
    }
    float s = 1.0f;
    if (ax > eps)
    {
        s = std::fminf(s, maneuverMax / ax);
    }
    if (ay > eps)
    {
        s = std::fminf(s, mainMax / ay);
    }
    local.x = tx * s;
    local.y = ty * s;
}

struct PhyThrust
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "phy-thrust";

    glm::vec2 thrustGlobal;
    glm::vec2 thrustLocal;
    float torque;
    float maxTorque;
    float maxRotVel;
    float thrustMainMax;
    float thrustManeuverMax;
    float maxSpd;

    void setThrustGlobal(glm::vec2 th, Transform& tr)
    {
        thrustLocal = smath::rotateVec2(th, tr.rot);
        clampThrustLocalToActuatorBox(
            thrustLocal, thrustManeuverMax, thrustMainMax);
        thrustGlobal = smath::rotateVec2(thrustLocal, -tr.rot);
    }

    void setThrustLocal(glm::vec2 th, Transform& tr)
    {
        thrustLocal = th;
        clampThrustLocalToActuatorBox(
            thrustLocal, thrustManeuverMax, thrustMainMax);
        thrustGlobal = smath::rotateVec2(thrustLocal, -tr.rot);
    }

    void setTorque(float trq)
    {
        trq = std::clamp(trq, -maxTorque, maxTorque);
        torque = trq;
    }
    void setTorqueNorm(float trq)
    {
        trq = std::clamp(trq, -1.0f, 1.0f);
        torque = trq * maxTorque;
    }

    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node)
    {
        PhyThrust phyThrust;
        TRY_YAML_DICT(phyThrust.thrustGlobal,
                      node["thrustGlobal"],
                      glm::vec2(0.0f, 0.0f));
        TRY_YAML_DICT(
            phyThrust.thrustLocal, node["thrustLocal"], glm::vec2(0.0f, 0.0f));
        TRY_YAML_DICT(phyThrust.torque, node["torque"], 0.0f);
        TRY_YAML_DICT(phyThrust.maxTorque, node["maxTorque"], 0.0f);
        TRY_YAML_DICT(phyThrust.maxRotVel, node["maxRotVel"], 0.0f);
        TRY_YAML_DICT(phyThrust.thrustMainMax, node["thrustMainMax"], 0.0f);
        TRY_YAML_DICT(
            phyThrust.thrustManeuverMax, node["thrustManeuverMax"], 0.0f);
        TRY_YAML_DICT(phyThrust.maxSpd, node["maxSpd"], 0.0f);
        registry.emplace<PhyThrust>(entity, phyThrust);
    }
};

#define SER_PHY_THRUST                                                         \
    SOBJ(o.thrustGlobal);                                                      \
    SOBJ(o.thrustLocal);                                                       \
    S4b(o.torque);                                                             \
    S4b(o.maxTorque);                                                          \
    S4b(o.maxRotVel);                                                          \
    S4b(o.thrustMainMax);                                                      \
    S4b(o.thrustManeuverMax);                                                  \
    S4b(o.maxSpd);
EXT_SER(PhyThrust, SER_PHY_THRUST)
EXT_DES(PhyThrust, SER_PHY_THRUST)

struct MoveCtrl
{
    enum class FaceDirMode : uint8_t
    {
        None,
        Forward,
        TargetPoint,
    };

    static const uint16_t VERSION = 1;
    static constexpr string NAME = "move-ctrl";

    bool active = false;
    FaceDirMode faceDirMode;
    def::SectorCoords spPos;
    // lookAt only works in sector
    vec2 lookAt;
    float spRot;

    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node)
    {
        MoveCtrl c;
        string dirMode;
        TRY_YAML_DICT(c.active, node["active"], false);
        TRY_YAML_DICT(c.spPos.sectorPos.x, node["spPos"][0], 0.0f);
        TRY_YAML_DICT(c.spPos.sectorPos.y, node["spPos"][1], 0.0f);
        TRY_YAML_DICT(c.spPos.pos.x, node["spPosSec"][0], 0u);
        TRY_YAML_DICT(c.spPos.pos.y, node["spPosSec"][1], 0u);
        TRY_YAML_DICT(c.spRot, node["spRot"], 0.0f);
        TRY_YAML_DICT(dirMode, node["faceDirMode"], "None");
        auto faceDirMode = magic_enum::enum_cast<FaceDirMode>(dirMode);
        if(faceDirMode.has_value())
        {
            c.faceDirMode = faceDirMode.value();
        }
        else
        {
            c.faceDirMode = FaceDirMode::None;
        }
        TRY_YAML_DICT(c.lookAt.x, node["lookAt"][0], 0.0f);
        TRY_YAML_DICT(c.lookAt.y, node["lookAt"][1], 0.0f);
        registry.emplace<MoveCtrl>(entity, c);
    }
};

#define SER_MOVE_CTRL_HOLD                                                     \
    S1b(o.active);                                                             \
    SOBJ(o.spPos);                                                             \
    S4b(o.spRot);                                                              \
    S1b(o.faceDirMode);                                                        \
    SOBJ(o.lookAt);
EXT_SER(MoveCtrl, SER_MOVE_CTRL_HOLD)
EXT_DES(MoveCtrl, SER_MOVE_CTRL_HOLD)

}  // namespace ecs

EXT_FMT(ecs::Transform, "(pos: {}, rot: {})", o.pos, o.rot);

EXT_FMT(ecs::TransformNet, "{}", o.transform);

EXT_FMT(ecs::PhysicsBody,
        "(mass: {}, inertia: {}, vel: {}, acc: {}, rotVel: {}, rotAcc: {})",
        o.mass,
        o.inertia,
        o.vel,
        o.acc,
        o.rotVel,
        o.rotAcc);
EXT_FMT(ecs::PhyThrust,
        "(thrustGlobal: {}, thrustLocal: {}, torque: {}, maxTorque: {}, "
        "maxRotVel: {}, thrustMainMax: {}, thrustManeuverMax: {}, maxSpd: {})",
        o.thrustGlobal,
        o.thrustLocal,
        o.torque,
        o.maxTorque,
        o.maxRotVel,
        o.thrustMainMax,
        o.thrustManeuverMax,
        o.maxSpd);
EXT_FMT(ecs::MoveCtrl,
        "(active: {}, spPos: {}, spRot: {}, faceDirMode: {}, lookAt: {})",
        o.active,
        o.spPos,
        o.spRot,
        magic_enum::enum_name(o.faceDirMode),
        o.lookAt);

#endif