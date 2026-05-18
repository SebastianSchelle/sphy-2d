#ifndef UI_TAB_PANEL_HPP
#define UI_TAB_PANEL_HPP

#include <std-inc.hpp>
#include "RmlUi/Core/DataModelHandle.h"
#include "RmlUi/Core/Event.h"

namespace ui
{

class UserInterface;

struct UiTab
{
    string title;
    string documentId;
};

class UiTabPanel
{
  public:
    UiTabPanel(UserInterface* userInterface, const string& id);
    ~UiTabPanel();
    void init();
    void addTab(const string& title, const string& documentId);
    void selectTab(const string& documentId);
    void hideCurrentTab();

  private:
    void setupDataModel();
    void onTabSelected(Rml::DataModelHandle handle,
                       Rml::Event& event,
                       const Rml::VariantList& args);

    UserInterface* userInterface;
    vector<UiTab> tabs;
    int currentTab = -1;
    Rml::DataModelHandle dataModelHandle;
    string id;
};


}  // namespace ui


#endif