#ifndef COMP_STORAGE_HPP
#define COMP_STORAGE_HPP

#include <lib-modules.hpp>
#include <std-inc.hpp>
#include <entt/entt.hpp>

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

struct Storage
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "storage";

    CargoVolume cargo[static_cast<size_t>(gobj::StorageType::NumStorageTypes)];

    void updateStatsFromEntity(entt::entity entity, ecs::PtrHandle* ptrHandle);
};

#define SER_STORAGE SOBJ(o.cargo);
EXT_SER(Storage, SER_STORAGE)
EXT_DES(Storage, SER_STORAGE)

}  // namespace ecs

#endif