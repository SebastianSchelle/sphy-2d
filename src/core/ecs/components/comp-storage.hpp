#ifndef COMP_STORAGE_HPP
#define COMP_STORAGE_HPP

#include <std-inc.hpp>

namespace ecs
{

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

    CargoVolume containerS;
    CargoVolume containerL;
    CargoVolume tank;
    CargoVolume bulk;
};

#define SER_STORAGE                                                            \
    SOBJ(o.containerS);                                                        \
    SOBJ(o.containerL);                                                        \
    SOBJ(o.tank);                                                              \
    SOBJ(o.bulk);
EXT_SER(Storage, SER_STORAGE)
EXT_DES(Storage, SER_STORAGE)

}  // namespace ecs

#endif