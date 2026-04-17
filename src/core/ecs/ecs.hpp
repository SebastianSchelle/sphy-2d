#ifndef ECS_HPP
#define ECS_HPP

#include <asset-factory.hpp>
#include <entt/entt.hpp>
#include <ptr-handle.hpp>
#include <std-inc.hpp>
#include <comp-ident.hpp>

namespace world
{
  class Sector;
}

namespace ecs
{

typedef std::function<
    void(world::Sector*, entt::entity, const ecs::EntityId&, float, std::shared_ptr<PtrHandle>)>
    SFSectorForeach;
typedef std::function<
    void(world::Sector*, float, std::shared_ptr<PtrHandle>)>
    SFSectorOnce;

enum class SystemType : uint8_t
{
    SectorForeachEntitiy,
    SectorOnce,
};

using SystemFunction = std::variant<SFSectorForeach, SFSectorOnce>;

struct System
{
    std::string name;
    SystemType type;
    SystemFunction function;
    bool afterEntityUpdate = false;
    bool operator==(const System& other) const
    {
        return name == other.name;
    }
    bool operator!=(const System& other) const
    {
        return !(*this == other);
    }
};

struct Slot
{
    entt::entity entity;
    uint16_t generation;
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
    EntityId getEntityIdFromIdx(uint32_t index);
    const vector<System>& getRegisteredSystems();
    void registerSystem(const System system);
    bool spawnEntityFromAsset(EntityId entityId,
                              const std::string& assetId,
                              const AssetFactory& assetFactory);
    entt::registry& getRegistry();
    entt::entity insertOrReplaceByExistingEntityId(EntityId entityId);

  private:
    entt::registry registry;
    vector<Slot> idMap;
    vector<uint32_t> idMapFreeSlots;
    vector<System> registeredSystems;
};

class EcsClient
{
  public:
    EcsClient();
    ~EcsClient();
    entt::registry& getRegistry();
    entt::entity enttFromServerId(const EntityId& entityId);

  private:
    entt::registry registry;
    std::unordered_map<uint32_t, Slot> idMap;
};

}  // namespace ecs

EXT_FMT(entt::entity, "{}", static_cast<std::uint32_t>(o));

#endif