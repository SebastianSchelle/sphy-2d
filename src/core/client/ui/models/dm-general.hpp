#ifndef DM_GENERAL_HPP
#define DM_GENERAL_HPP

#include "RmlUi/Core/DataModelHandle.h"
#include "save-manager.hpp"
#include "widgets.hpp"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <std-inc.hpp>

namespace ui
{

struct DmTips
{
    string tip;

    static void RegisterType(Rml::DataModelConstructor& constructor)
    {
        if (auto handle = constructor.RegisterStruct<DmTips>())
        {
            handle.RegisterMember("tip", &DmTips::tip);
        }
    }
};

struct DmSaveInfo
{
    widget::Button button;
    string path;

    static void RegisterType(Rml::DataModelConstructor& constructor)
    {
        widget::Button::RegisterType(constructor);
        if (auto handle = constructor.RegisterStruct<DmSaveInfo>())
        {
            handle.RegisterMember("button", &DmSaveInfo::button);
        }
    }
};

struct DmLoadGame
{
    vector<DmSaveInfo> saves;

    static void RegisterType(Rml::DataModelConstructor& constructor)
    {
        DmSaveInfo::RegisterType(constructor);
        constructor.RegisterArray<vector<DmSaveInfo>>();
        if (auto handle = constructor.RegisterStruct<DmLoadGame>())
        {
            handle.RegisterMember("saves", &DmLoadGame::saves);
        }
    }
};

struct DmMenu
{
    DmLoadGame load;

    static void RegisterType(Rml::DataModelConstructor& constructor)
    {
        DmLoadGame::RegisterType(constructor);
        if (auto handle = constructor.RegisterStruct<DmMenu>())
        {
            handle.RegisterMember("load", &DmMenu::load);
        }
    }
};

}  // namespace ui

#endif