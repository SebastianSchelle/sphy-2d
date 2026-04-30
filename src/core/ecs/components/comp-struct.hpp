#ifndef COMP_STRUCT_HPP
#define COMP_STRUCT_HPP

#include "comp-ident.hpp"
#include "std-inc.hpp"
#include <magic_enum/magic_enum.hpp>

namespace gobj
{
enum class ModuleType : uint8_t;
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

}  // namespace ecs

EXT_FMT(ecs::Hull, "(hullHandle: {}, hull: {})", o.hullHandle, o.hp);

#endif