#ifndef ECS_HPP
#define ECS_HPP

#include <entt/entt.hpp>
#include <std-inc.hpp>

namespace ecs
{
struct PtrHandle;

typedef std::function<
    void(entt::entity entity, float dt, std::shared_ptr<PtrHandle> ptrHandle)>
    SystemFunction;

struct System
{
    std::string name;
    SystemFunction function;
    bool operator==(const System& other) const
    {
        return name == other.name;
    }
    bool operator!=(const System& other) const
    {
        return !(*this == other);
    }
};

struct EntityId
{
    uint32_t index;
    uint32_t generation;
};

struct Slot
{
    entt::entity entity;
    uint32_t generation;
};

class Ecs
{
  public:
    Ecs();
    ~Ecs();
    EntityId createEntity();
    void destroyEntity(EntityId entityId);
    bool validId(EntityId entityId);
    entt::entity getEntity(EntityId entityId);
    const vector<System>& getRegisteredSystems();
    void registerSystem(System system);

  private:
    entt::registry registry;
    vector<Slot> idMap;
    vector<uint32_t> idMapFreeSlots;
    vector<System> registeredSystems;
};

}  // namespace ecs

#endif