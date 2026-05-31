#ifndef COMP_STORAGE_HPP
#define COMP_STORAGE_HPP

#include <lib-modules.hpp>
#include <std-inc.hpp>
#include <entt/entt.hpp>
#include <lib-item.hpp>

namespace ecs
{
struct PtrHandle;

struct CargoVolume
{
    float capacity;
    float used;
};

#define SER_CARGO_VOLUME                                                       \
    S4b(o.capacity);                                                           \
    S4b(o.used);
EXT_SER(CargoVolume, SER_CARGO_VOLUME)
EXT_DES(CargoVolume, SER_CARGO_VOLUME)

struct StorageSlot
{
    GenericHandle itemHandle;
    uint32_t quantity;
};

#define SER_STORAGE_SLOT                                                       \
    SOBJ(o.itemHandle);                                                        \
    S4b(o.quantity);
EXT_SER(StorageSlot, SER_STORAGE_SLOT)
EXT_DES(StorageSlot, SER_STORAGE_SLOT)

struct Storage
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "storage";

    CargoVolume cargo[static_cast<size_t>(gobj::ItemStorageType::NumStorageTypes)];
    vector<StorageSlot> slots;

    void updateStatsFromEntity(entt::entity entity, ecs::PtrHandle* ptrHandle);
    uint32_t tryAddItem(const gobj::ItemHandle& itemHandle, const gobj::Item& item, uint32_t quantity);
};

#define SER_STORAGE                                                          \
    SOBJ(o.slots); \
    for (size_t _i = 0; _i < static_cast<size_t>(gobj::ItemStorageType::NumStorageTypes); ++_i) { \
        s.object(o.cargo[_i]); \
    }
EXT_SER(Storage, SER_STORAGE)
EXT_DES(Storage, SER_STORAGE)

}  // namespace ecs

#endif