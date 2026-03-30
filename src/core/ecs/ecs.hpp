#ifndef ECS_HPP
#define ECS_HPP

#include <asset-factory.hpp>
#include <entt/entt.hpp>
#include <std-inc.hpp>
#include <ptr-handle.hpp>

namespace util
{
struct PtrHandle;
}

namespace ecs
{

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
    bool operator==(const EntityId& other) const
    {
        return index == other.index && generation == other.generation;
    }
    bool operator!=(const EntityId& other) const
    {
        return !(*this == other);
    }
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
    void registerSystem(const System system);
    bool spawnEntityFromAsset(EntityId entityId,
                              const std::string& assetId,
                              const AssetFactory& assetFactory);
    entt::registry& getRegistry();

  private:
    entt::registry registry;
    vector<Slot> idMap;
    vector<uint32_t> idMapFreeSlots;
    vector<System> registeredSystems;
};

}  // namespace ecs

template <> struct fmt::formatter<entt::entity>
{
    constexpr auto parse(fmt::format_parse_context& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const entt::entity& e, FormatContext& ctx) const
    {
        // entt::entity is an enum class; format its underlying numeric id.
        return fmt::format_to(ctx.out(), "{}", static_cast<std::uint32_t>(e));
    }
};

template <> struct fmt::formatter<ecs::EntityId>
{
    constexpr auto parse(fmt::format_parse_context& ctx)
    {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const ecs::EntityId& e, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), "{}:{}", e.index, e.generation);
    }
};

#endif