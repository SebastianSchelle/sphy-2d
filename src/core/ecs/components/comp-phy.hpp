#ifndef COMP_PHY_HPP
#define COMP_PHY_HPP

#include <entt/entt.hpp>
#include <std-inc.hpp>
#include <yaml-cpp/yaml.h>

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
        thrustLocal = hmath::rotateVec2(th, tr.rot);
        thrustLocal[0] =
            std::clamp(thrustLocal[0], -thrustManeuverMax, thrustManeuverMax);
        thrustLocal[1] =
            std::clamp(thrustLocal[1], -thrustMainMax, thrustMainMax);
        thrustGlobal = hmath::rotateVec2(thrustLocal, -tr.rot);
    }

    void setThrustLocal(glm::vec2 th, Transform& tr)
    {
        thrustLocal = th;
        thrustLocal[0] =
            std::clamp(thrustLocal[0], -thrustManeuverMax, thrustManeuverMax);
        thrustLocal[1] =
            std::clamp(thrustLocal[1], -thrustMainMax, thrustMainMax);
        thrustGlobal = hmath::rotateVec2(thrustLocal, -tr.rot);
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

struct PhyPid
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "phy-pid";

    bool active = false;
    vec2 spPos;
    float spRot;

    ctrl::PD pdFwd;
    ctrl::PD pdSide;

#ifdef DEBUG
    float spVelX;
    float spVelY;
    float spRotVel;
#endif

    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node)
    {
        PhyPid c;
        TRY_YAML_DICT(c.active, node["active"], false);
        TRY_YAML_DICT(c.spPos.x, node["spPos"][0], 0.0f);
        TRY_YAML_DICT(c.spPos.y, node["spPos"][1], 0.0f);
        TRY_YAML_DICT(c.spRot, node["spRot"], 0.0f);
        // Tuned for smoother approach: less derivative kick and stronger
        // rotational damping.
        float kpFwd; TRY_YAML_DICT(kpFwd, node["kpFwd"], 0.05f);
        float kdFwd; TRY_YAML_DICT(kdFwd, node["kdFwd"], 0.01f);
        float kpSide; TRY_YAML_DICT(kpSide, node["kpSide"], 0.05f);
        float kdSide; TRY_YAML_DICT(kpSide, node["kdSide"], 0.01f);
        ctrl::pdInit(&c.pdFwd, kpFwd, kdFwd);
        ctrl::pdInit(&c.pdSide, kpSide, kdSide);
        registry.emplace<PhyPid>(entity, c);
    }
};

#define SER_PHY_PID_HOLD                                                       \
    S1b(o.active);                                                             \
    SOBJ(o.spPos);                                                             \
    S4b(o.spRot);                                                              \
    S4b(o.pdFwd.prev_error);                                                   \
    S4b(o.pdSide.prev_error);

#define DES_PHY_PID_HOLD                                                       \
    S1b(o.active);                                                             \
    SOBJ(o.spPos);                                                             \
    S4b(o.spRot);                                                              \
    S4b(o.pdFwd.prev_error);                                                   \
    S4b(o.pdSide.prev_error);

EXT_SER(PhyPid, SER_PHY_PID_HOLD)
EXT_DES(PhyPid, DES_PHY_PID_HOLD)

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
EXT_FMT(ecs::PhyPid,
        "(active: {}, spPos: {}, spRot: {})",
        o.active,
        o.spPos,
        o.spRot);

#endif