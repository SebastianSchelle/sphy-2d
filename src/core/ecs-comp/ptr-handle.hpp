#ifndef PTR_HANDLE_HPP
#define PTR_HANDLE_HPP

#include <entt/entt.hpp>
#include <std-inc.hpp>
#ifdef SERVER
#include "systems.hpp"
namespace sphys
{
class Engine;
}
namespace ai
{
class TaskSystem;
}
namespace ecs
{
class CollisionLayerMat;
class AssetFactory;
}  // namespace ecs
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
class RegistryMapping;
struct System;

struct PtrHandle
{
    world::World* world;
    mod::ModManager* modManager;
    // Shared fields must precede SERVER/CLIENT-only members (see
    // cmake/SphyTargetKind.cmake).
    uint32_t frameCnt;
    RegistryMapping* registryMapping;
    sthread::WorkDistributor* workDistributor;
    con::ItemLib<gobj::Collider>* colliderLib = nullptr;
#ifdef SERVER
    Systems* systems;
    sphys::Engine* engine;
    float kpThrust;
    float kpTurn;
    float angDrag;
    float linDrag;
    float minFaceTargetDist;
    float miningRate;
    float itemLifetime;
    ai::TaskSystem* taskSystem;
    ecs::CollisionLayerMat* collisionLayerMat;
    ecs::AssetFactory* assetFactory;
#elif CLIENT
    sphyc::Client* client;
#endif
};

}  // namespace ecs

#endif
