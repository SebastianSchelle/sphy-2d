#ifndef PTR_HANDLE_HPP
#define PTR_HANDLE_HPP

#include <std-inc.hpp>
#include <world.hpp>
#include <engine.hpp>

namespace ecs
{

struct PtrHandle
{
    entt::registry registry;
    world::World* world;
    sphys::Engine* engine;
    vector<System>* systems;
};

}  // namespace ecs

#endif
