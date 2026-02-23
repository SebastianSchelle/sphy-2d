#ifndef PTR_HANDLE_HPP
#define PTR_HANDLE_HPP

#include <std-inc.hpp>
#include <world.hpp>
#ifdef SERVER
#include <engine.hpp>
#elif CLIENT
#include <client.hpp>
#endif

namespace ecs
{

struct PtrHandle
{
    entt::registry registry;
    world::World* world;
#ifdef SERVER
    sphys::Engine* engine;
#elif CLIENT
    sphyc::Client* client;
#endif
    vector<System>* systems;
};

}  // namespace ecs

#endif
