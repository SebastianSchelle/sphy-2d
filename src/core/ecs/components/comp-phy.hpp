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
        physicsBody.mass = node["mass"].as<float>();
        physicsBody.inertia = node["inertia"].as<float>();
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

#endif