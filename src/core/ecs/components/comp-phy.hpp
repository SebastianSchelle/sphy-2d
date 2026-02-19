#ifndef COMP_PHY_HPP
#define COMP_PHY_HPP

#include <std-inc.hpp>
#include <yaml-cpp/yaml.h>
#include <ecs.hpp>

namespace ecs
{

struct Transform
{
    vec2 pos;
    float rot;

    static void fromYaml(entt::registry& registry, entt::entity entity, const YAML::Node& node)
    {
        Transform transform;
        transform.pos = node["pos"].as<vec2>();
        transform.rot = node["rot"].as<float>();
        registry.emplace<Transform>(entity, transform);
    }
};

struct PhysicsBody
{
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
    static void fromYaml(entt::registry& registry, entt::entity entity, const YAML::Node& node)
    {
        PhysicsBody physicsBody;
        physicsBody.mass = node["mass"].as<float>();
        physicsBody.inertia = node["inertia"].as<float>();
        registry.emplace<PhysicsBody>(entity, physicsBody);
    }
};

}  // namespace ecs

#endif