#ifndef LIB_ITEM_HPP
#define LIB_ITEM_HPP

#include <std-inc.hpp>

namespace def
{

enum class StorageType : uint8_t
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
    StorageType storageType;
    float volume;
    float density;
    PriceRange priceRange;
};

}  // namespace def

#endif  // LIB_ITEM_HPP