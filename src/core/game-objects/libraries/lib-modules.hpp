#ifndef LIB_MODULES_HPP
#define LIB_MODULES_HPP

#include <std-inc.hpp>
#include <yaml-cpp/yaml.h>
#include <magic_enum/magic_enum.hpp>

namespace gobj
{

enum class ModuleSlotType : uint8_t
{
    ThrusterMainS_Common,
    ThrusterMainM_Common,
    ThrusterMainL_Common,
    ThrusterManeuverS_Common,
    ThrusterManeuverM_Common,
    ThrusterManeuverL_Common,
    InternalS_Common,
    InternalM_Common,
    InternalL_Common,
    RoofS_Common,
    RoofM_Common,
    RoofL_Common,
    BayS_Common,
    BayM_Common,
    BayL_Common,
};

struct ModuleSlot
{
    ModuleSlotType modType;
    vec2 pos;
    float rot;
    uint8_t z;
};

}  // namespace gobj

EXT_FMT(gobj::ModuleSlotType, "{}", magic_enum::enum_name(o));
EXT_FMT(gobj::ModuleSlot, "(modType: {}, pos: {}, rot: {}, z: {})", o.modType, o.pos, o.rot, o.z);

#endif  // LIB_MODULES_HPP