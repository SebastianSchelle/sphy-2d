#ifndef COMP_STRUCT_HPP
#define COMP_STRUCT_HPP

#include "comp-ident.hpp"
#include "std-inc.hpp"
#include <magic_enum/magic_enum.hpp>

namespace ecs
{

struct Hull
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "hull";

    GenericHandle hullHandle;
    float hull = 100.0f;
};

#define SER_HULL                                                               \
    SOBJ(o.hullHandle);                                                        \
    S4b(o.hull);
EXT_SER(Hull, SER_HULL)
EXT_DES(Hull, SER_HULL)

}  // namespace ecs

EXT_FMT(ecs::Hull, "(hullHandle: {}, hull: {})", o.hullHandle, o.hull);

#endif