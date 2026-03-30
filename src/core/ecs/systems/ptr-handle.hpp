#ifndef PTR_HANDLE_HPP
#define PTR_HANDLE_HPP

#include <std-inc.hpp>
#include <entt/entt.hpp>
#ifdef SERVER
namespace sphys
{
class Engine;
}
#elif CLIENT
namespace sphyc
{
class Client;
}
#endif

namespace world
{
class World;
}

namespace ecs
{
class Ecs;
struct System;

struct PtrHandle
{
    entt::registry* registry;
    world::World* world;
#ifdef SERVER
    sphys::Engine* engine;
#elif CLIENT
    sphyc::Client* client;
#endif
    const vector<System>* systems;
    ecs::Ecs* ecs;
};

}  // namespace ecs

#endif
