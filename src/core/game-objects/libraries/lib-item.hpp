#ifndef LIB_ITEM_HPP
#define LIB_ITEM_HPP

#include <std-inc.hpp>
#include <item-lib.hpp>
#include <magic_enum/magic_enum.hpp>

#ifdef CLIENT
#include <texture.hpp>
#endif

namespace mod
{
class ResourceMap;
}

namespace gobj
{

enum class ItemStorageType : uint8_t
{
    Cargo,
    Tank,
    ContainerS,
    ContainerL,
};

enum class ItemType : uint8_t
{
    Ore,
    Gas,
    Ammo,
    Food,
    Energy,
    Resource,
    Turret,
    Missile,
    Drone,
};

struct PriceRange
{
    float min;
    float max;
};

struct Item
{
    string name;
    string description;
    ItemType type;
    ItemStorageType storageType;
    float volume;
    float density;
    PriceRange priceRange;

    GenericHandle worldTexture = GenericHandle::Invalid();
    GenericHandle iconTexture = GenericHandle::Invalid();

    static Item fromYaml(const YAML::Node& node, mod::ResourceMap& resourceMap);
};

using ItemHandle = typename con::ItemLib<Item>::Handle;

}  // namespace gobj

EXT_FMT(gobj::ItemStorageType, "{}", magic_enum::enum_name(o));
EXT_FMT(gobj::ItemType, "{}", magic_enum::enum_name(o));
EXT_FMT(gobj::PriceRange, "(min: {}, max: {})", o.min, o.max);
EXT_FMT(gobj::Item,
        "(name: {}, description: {}, type: {}, storageType: {}, volume: {}, "
        "density: {}, priceRange: {})",
        o.name,
        o.description,
        o.type,
        o.storageType,
        o.volume,
        o.density,
        o.priceRange);

#endif  // LIB_ITEM_HPP