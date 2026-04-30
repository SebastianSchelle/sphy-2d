#ifndef PTR_HANDLE_HPP
#define PTR_HANDLE_HPP

#include <std-inc.hpp>
#include <entt/entt.hpp>
#ifdef SERVER
namespace sphys
{
class Engine;
}
namespace ai
{
class TaskSystem;
}
#elif CLIENT
namespace sphyc
{
class Client;
}
#endif

namespace sthread
{
class WorkDistributor;
}

namespace con
{
template <typename T> class ItemLib;
}

namespace gobj
{
struct Collider;
}

namespace world
{
class World;
}

namespace mod
{
class ModManager;
}

namespace ecs
{
class Ecs;
struct System;

struct PtrHandle
{
    entt::registry* registry;
    world::World* world;
    mod::ModManager* modManager;
#ifdef SERVER
    sphys::Engine* engine;
    float kpThrust;
    float kpTurn;
    float angDrag;
    float linDrag;
    float minFaceTargetDist;
    ai::TaskSystem* taskSystem;
#elif CLIENT
    sphyc::Client* client;
#endif
    uint32_t frameCnt;
    const vector<System>* systems;
    ecs::Ecs* ecs;
    sthread::WorkDistributor* workDistributor;
    con::ItemLib<gobj::Collider>* colliderLib = nullptr;
};

}  // namespace ecs

#endif
