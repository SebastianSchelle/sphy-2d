#ifndef COMP_STRUCT_HPP
#define COMP_STRUCT_HPP

#include "comp-ident.hpp"
#include "std-inc.hpp"
#include <magic_enum/magic_enum.hpp>
#include <lib-modules.hpp>

namespace gobj
{
enum class ModuleType : uint8_t;
enum class StationPartType : uint8_t;
}

namespace ecs
{

struct ModuleRef
{
    EntityId entityId;
    gobj::ModuleType slotType;
};

#define SER_MODULE_REF                                                         \
    SOBJ(o.entityId);                                                          \
    S1b(o.slotType);
EXT_SER(ModuleRef, SER_MODULE_REF)
EXT_DES(ModuleRef, SER_MODULE_REF)

struct Hull
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "hull";

    Hull() : hp(0.0f) {}
    Hull(size_t slotCnt, float hp, GenericHandle hullHandle) : hp(hp), hullHandle(hullHandle)
    {
        modules.resize(slotCnt);
        for (size_t i = 0; i < slotCnt; i++)
        {
            modules[i] = ModuleRef{EntityId::Invalid(), gobj::ModuleType::None};
        }
    }

    bool addModule(uint16_t idx, const ModuleRef& module)
    {
        if (idx >= modules.size())
        {
            return false;
        }
        modules[idx] = module;
        return true;
    }

    // todo: replace with something efficient
    std::vector<ModuleRef> modules;
    GenericHandle hullHandle;
    float hp;
};

#define SER_HULL                                                               \
    SOBJ(o.hullHandle);                                                        \
    S4b(o.hp);                                                                 \
    SOBJ(o.modules);
EXT_SER(Hull, SER_HULL)
EXT_DES(Hull, SER_HULL)

struct Module
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "module";

    GenericHandle moduleHandle;
};

#define SER_MODULE SOBJ(o.moduleHandle);
EXT_SER(Module, SER_MODULE)
EXT_DES(Module, SER_MODULE)

struct StationPartRef
{
    EntityId entityId;
    gobj::StationPartType partType;
};

#define SER_STATION_PART_REF                                                         \
    SOBJ(o.entityId);                                                          \
    S1b(o.partType);
EXT_SER(StationPartRef, SER_STATION_PART_REF)
EXT_DES(StationPartRef, SER_STATION_PART_REF)

struct Station
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "station";

    std::vector<StationPartRef> stationParts;
    float hp;
};

#define SER_STATION                                                            \
    SOBJ(o.stationParts);                                                      \
    S4b(o.hp);
EXT_SER(Station, SER_STATION)
EXT_DES(Station, SER_STATION)

struct StationPart
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "station-part";

    GenericHandle stationPartHandle;
};

#define SER_STATION_PART SOBJ(o.stationPartHandle);
EXT_SER(StationPart, SER_STATION_PART)
EXT_DES(StationPart, SER_STATION_PART)

}  // namespace ecs

EXT_FMT(ecs::Hull, "(hullHandle: {}, hull: {})", o.hullHandle, o.hp);
EXT_FMT(ecs::Module, "(moduleHandle: {})", o.moduleHandle);
EXT_FMT(ecs::Station, "(hp: {})", o.hp);
EXT_FMT(ecs::StationPart, "(stationPartHandle: {})", o.stationPartHandle);

#endif