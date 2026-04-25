#ifndef COMP_SHIP_HPP
#define COMP_SHIP_HPP

#include "comp-ident.hpp"
#include "std-inc.hpp"
#include <magic_enum/magic_enum.hpp>

namespace ecs
{
namespace ship
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

struct Module
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "ship-module";

    ModuleSlotType modType;

    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node,
                         mod::ResourceMap& resourceMap)
    {
        Module module;
        string modTypeStr;
        TRY_YAML_DICT(modTypeStr, node["mod-type"], "CommonInternalS");
        module.modType = magic_enum::enum_cast<ModuleSlotType>(modTypeStr).value();
        registry.emplace<Module>(entity, module);
    }
};

#define SER_MODULE S1b(o.modType);
EXT_SER(Module, SER_MODULE)
EXT_DES(Module, SER_MODULE)

struct ModuleSlot
{
    ModuleSlotType modType;
    EntityId ref;
    vec2 pos;
    float rot;
    uint8_t z;
};

#define SER_MODULE_SLOT                                                          \
    S1b(o.modType);                                                            \
    SOBJ(o.ref);                                                               \
    SOBJ(o.pos);                                                               \
    S4b(o.rot);
EXT_SER(ModuleSlot, SER_MODULE_SLOT)
EXT_DES(ModuleSlot, SER_MODULE_SLOT)

struct Hull
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "ship-hull";

    float hull = 100.0f;
    float hullMax = 100.0f;

    std::vector<ModuleSlot> slots;

    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node,
                         mod::ResourceMap& resourceMap)
    {
        Hull hull;
        TRY_YAML_DICT(hull.hullMax, node["hullMax"], 100.0f);
        TRY_YAML_DICT(hull.hull, node["hull"], hull.hullMax);

        for (const auto& slotNode : node["slots"])
        {
            ModuleSlot slot;
            string modType;
            TRY_YAML_DICT(modType, slotNode["mod-type"], "CommonInternalS");
            slot.modType =
                magic_enum::enum_cast<ModuleSlotType>(modType).value();
            uint16_t refIdx;
            uint16_t refGen;
            TRY_YAML_DICT(refIdx, slotNode["ref"][0], 0);
            TRY_YAML_DICT(refGen, slotNode["ref"][1], 0);
            slot.ref = {refIdx, refGen};
            TRY_YAML_DICT(slot.pos, slotNode["pos"], vec2(0.0f, 0.0f));
            float rot;
            TRY_YAML_DICT(rot, slotNode["rot"], 0.0f);
            slot.rot = smath::degToRad(rot);
            hull.slots.push_back(slot);
        }
        registry.emplace<Hull>(entity, hull);
    }
};

#define SER_HULL                                                               \
    S4b(o.hull);                                                               \
    S4b(o.hullMax);                                                            \
    SOBJ(o.slots);
EXT_SER(Hull, SER_HULL)
EXT_DES(Hull, SER_HULL)

}  // namespace ship
}  // namespace ecs

EXT_FMT(ecs::ship::Hull, "(hull: {}, hullMax: {}, slots: {})", o.hull, o.hullMax, o.slots);
EXT_FMT(ecs::ship::ModuleSlot, "(modType: {}, ref: {}, pos: {}, rot: {}, z: {})", o.modType, o.ref, o.pos, o.rot, o.z);
EXT_FMT(ecs::ship::Module, "(modType: {})", o.modType);


#endif