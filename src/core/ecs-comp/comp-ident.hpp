#ifndef COMP_IDENT_HPP
#define COMP_IDENT_HPP

#include "world-def.hpp"
#include <entt/entt.hpp>
#include <std-inc.hpp>
#include <yaml-cpp/yaml.h>

namespace mod
{
class ResourceMap;
}

namespace ecs
{

struct EntityId
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "entity-id";

    EntityId() : index(0), generation(0) {}
    EntityId(uint32_t index, uint16_t generation)
        : index(index), generation(generation)
    {
    }
    EntityId(GenericHandle32 genericHandle32)
        : index(genericHandle32.idx), generation(genericHandle32.gen)
    {
    }

    uint32_t index;
    uint16_t generation;
    bool operator==(const EntityId& other) const
    {
        return index == other.index && generation == other.generation;
    }
    bool operator!=(const EntityId& other) const
    {
        return !(*this == other);
    }
    static const EntityId Invalid()
    {
        return {0, 0};
    }
    GenericHandle32 toGenericHandle32() const
    {
        return {index, generation};
    }
};

struct EntityIdHash
{
    std::size_t operator()(ecs::EntityId const& id) const noexcept
    {
        return std::hash<uint64_t>{}((uint64_t{id.index} << 32)
                                     | id.generation);
    }
};

#define SER_ENTITY_ID                                                          \
    S4b(o.index);                                                              \
    S2b(o.generation);
EXT_SER(EntityId, SER_ENTITY_ID)
EXT_DES(EntityId, SER_ENTITY_ID)

struct Flags
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "flags";

    enum Flag
    {
        None = 0x0000,
        Destroyed = 0x0001,
        Moved = 0x0002,
        MovedOrDestroyed = Destroyed | Moved,
    };
    using Flag_t = uint16_t;

    Flags() : flags(0) {}
    Flag_t flags;

    bool hasFlag(Flag f)
    {
        return !!(flags & (Flag_t)f);
    }
    void setFlag(Flag f)
    {
        flags |= (Flag_t)f;
    }
    void removeFlag(Flag f)
    {
        flags &= ~(Flag_t)f;
    }
};

#define SER_FLAGS S2b(o.flags);
EXT_SER(Flags, SER_FLAGS)
EXT_DES(Flags, SER_FLAGS)

struct AssetId
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "asset-id";

    std::string name;

    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node,
                         mod::ResourceMap& resourceMap)
    {
        AssetId assetId;
        assetId.name = node["name"].as<std::string>();
        registry.emplace<AssetId>(entity, assetId);
    }
};

// Bitsery: strings use text1b, not value4b (fundamental types only).
#define SER_ASSET_ID s.text1b(o.name, 512);
EXT_SER(AssetId, SER_ASSET_ID)
EXT_DES(AssetId, SER_ASSET_ID)


struct SectorId
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "sector-id";

    uint32_t id;
    def::SectorPos coord;

    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node,
                         mod::ResourceMap& resourceMap)
    {
        SectorId sectorId;
        TRY_YAML_DICT(sectorId.id, node["id"], 0u);
        TRY_YAML_DICT(sectorId.coord.x, node["x"], 0u);
        TRY_YAML_DICT(sectorId.coord.y, node["y"], 0u);
        registry.emplace<SectorId>(entity, sectorId);
    }
};

#define SER_SECTOR_ID                                                          \
    S4b(o.id);                                                                 \
    SOBJ(o.coord);
EXT_SER(SectorId, SER_SECTOR_ID)
EXT_DES(SectorId, SER_SECTOR_ID)

}  // namespace ecs

EXT_FMT(ecs::EntityId, "{}:{}", o.index, o.generation);
EXT_FMT(ecs::AssetId, "{}", o.name);
EXT_FMT(ecs::SectorId, "s:{}, x:{}, y:{}", o.id, o.coord.x, o.coord.y);

#endif