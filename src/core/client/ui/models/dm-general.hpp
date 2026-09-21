#ifndef DM_GENERAL_HPP
#define DM_GENERAL_HPP

#include "RmlUi/Core/DataModelHandle.h"
#include "save-manager.hpp"
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

struct DmLoadGame
{
    vector<sphyc::SaveInfo> saves;

    static void RegisterType(Rml::DataModelConstructor& constructor)
    {
        if (auto handle = constructor.RegisterStruct<sphyc::SaveInfo>())
        {
            handle.RegisterMember("name", &sphyc::SaveInfo::name);
            handle.RegisterMember("path", &sphyc::SaveInfo::path);
        }
        constructor.RegisterArray<vector<sphyc::SaveInfo>>();

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