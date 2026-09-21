#ifndef DM_IF_HPP
#define DM_IF_HPP

#include "RmlUi/Core/Context.h"
#include "RmlUi/Core/Core.h"
#include "RmlUi/Core/DataModelHandle.h"
#include "ptr-handle.hpp"
#include <dm-general.hpp>
#include <dm-window.hpp>

namespace ui
{
class DmIf
{
  public:
    DmIf(ecs::PtrHandle* ptrHandle)
        : ptrHandle(ptrHandle)
    {
    }
    void init(Rml::Context* context)
    {
        ui = ptrHandle->userInterface;
        rmlContext = context;
    }

    void setupDataModels();
    void postInit();

    // Data models
    DmWindow<DmMenu> menu;
    DmWindow<DmTips> tips;
    // DmWindow<DmLoadGame> loadGame;
    // Rml models
    Rml::DataModelHandle hMenu;
    Rml::DataModelHandle hTips;
    // Rml::DataModelHandle hLoadGame;

  private:
    Rml::Context* rmlContext;
    ui::UserInterface* ui;
    ecs::PtrHandle* ptrHandle;

    PageEventListener evLoadGame;

    void setupMenu();
    void setupTips();
    // void setupLoadGame();
};

}  // namespace ui

#endif