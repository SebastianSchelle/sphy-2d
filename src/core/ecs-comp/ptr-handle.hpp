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
class ComponentFactory;
}  // namespace ecs
#elif CLIENT
#include <localisation.hpp>
namespace sphyc
{
class Client;
}
namespace ui
{
class UserInterface;
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
#ifdef SERVER
    sthread::WorkDistributor* workDistributor;
    con::ItemLib<gobj::Collider>* colliderLib = nullptr;
    RegistryMapping* registryMapping;
    Systems* systems;
    sphys::Engine* engine;
    float kpThrust;
    float kpTurn;
    float angDrag;
    float linDrag;
    float minFaceTargetDist;
    float miningRate;
    float itemLifetime;
    float volumeMultiplier;
    ai::TaskSystem* taskSystem;
    ecs::CollisionLayerMat* collisionLayerMat;
    ecs::ComponentFactory* componentFactory;
#elif CLIENT
    sphyc::Client* client;
    ui::Localisation* locale;
    ui::UserInterface* userInterface;
#endif
};

}  // namespace ecs

#endif
