#ifndef DM_GENERAL_HPP
#define DM_GENERAL_HPP

#include <std-inc.hpp>
#include "RmlUi/Core/DataModelHandle.h"

namespace ui
{

struct DmTips
{
    string tip;

    static void RegisterType(Rml::DataModelConstructor& constructor) {
        if (auto handle = constructor.RegisterStruct<DmTips>())
        {
            handle.RegisterMember("tip", &DmTips::tip);
        }
    }
};

}  // namespace ui

#endif