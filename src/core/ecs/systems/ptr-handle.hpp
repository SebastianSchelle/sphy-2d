#ifndef PTR_HANDLE_HPP
#define PTR_HANDLE_HPP

#include <std-inc.hpp>
#include <world.hpp>
#include <ecs.hpp>
#ifdef SERVER
#include <engine.hpp>
#elif CLIENT
#include <client.hpp>
#endif

namespace ecs
{

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
