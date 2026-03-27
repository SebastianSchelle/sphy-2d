#ifndef COMP_PHY_HPP
#define COMP_PHY_HPP

#include <entt/entt.hpp>
#include <std-inc.hpp>
#include <yaml-cpp/yaml.h>

namespace ecs
{

struct Transform
{
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

}  // namespace ecs


template <> struct fmt::formatter<ecs::Transform>
{
    constexpr auto parse(fmt::format_parse_context& ctx)
    {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const ecs::Transform& transform, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(),
                              "Transform(pos: {}, rot: {})",
                              transform.pos,
                              transform.rot);
    }
};

template <> struct fmt::formatter<ecs::PhysicsBody>
{
    constexpr auto parse(fmt::format_parse_context& ctx)
    {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const ecs::PhysicsBody& physicsBody, FormatContext& ctx) const
    {
        return fmt::format_to(
            ctx.out(),
            "PhysicsBody(mass: {}, inertia: {}, vel: {}, rotVel: {})",
            physicsBody.mass,
            physicsBody.inertia,
            physicsBody.vel,
            physicsBody.rotVel);
    }
};

#endif