#ifndef COMP_TAG_HPP
#define COMP_TAG_HPP

#include <entt/entt.hpp>
#include <std-inc.hpp>
#include <yaml-cpp/yaml.h>

namespace mod
{
class ResourceMap;
}

namespace ecs
{

namespace tag
{

namespace obj
{
struct Ship
{
    static constexpr string NAME = "to-ship";
};
}  // namespace obj

namespace mod
{
struct Turret
{
    static constexpr string NAME = "tm-turret";
};

struct MainThruster
{
    static constexpr string NAME = "tm-thr-main";
};

struct ManeuverThruster
{
    static constexpr string NAME = "tm-thr-man";
};
}  // namespace mod


namespace item
{
    struct Item
    {
        static constexpr string NAME = "ti-item";
    };
    struct Ore
    {
        static constexpr string NAME = "ti-ore";
    };
    struct Gas
    {
        static constexpr string NAME = "ti-gas";
    };
    struct Liquid
    {
        static constexpr string NAME = "ti-liquid";
    };
    struct Container
    {
        static constexpr string NAME = "ti-container";
    };
}

}  // namespace tag
}  // namespace ecs

EXT_FMT(ecs::tag::obj::Ship, "tag-ship", 0);
EXT_FMT(ecs::tag::mod::Turret, "tag-turret", 0);
EXT_FMT(ecs::tag::mod::MainThruster, "tag-main-thruster", 0);
EXT_FMT(ecs::tag::mod::ManeuverThruster, "tag-maneuver-thruster", 0);

#endif