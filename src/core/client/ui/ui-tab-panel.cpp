#include "ui-tab-panel.hpp"
#include "user-interface.hpp"

namespace ui
{

UiTabPanel::UiTabPanel(UserInterface* userInterface, const string& id)
    : userInterface(userInterface), id(id)
{
}

UiTabPanel::~UiTabPanel() {}

void UiTabPanel::init()
{
    setupDataModel();
}

void UiTabPanel::addTab(const string& title, const string& documentId)
{
    auto it = std::find_if(tabs.begin(),
                           tabs.end(),
                           [&](const UiTab& tab)
                           { return tab.documentId == documentId; });
    if (it != tabs.end())
    {
        // Replace the tab
        it->title = title;
        it->documentId = documentId;
    }
    else
    {
        tabs.push_back(UiTab(title, documentId));
        dataModelHandle.DirtyVariable("tabs");
    }
}

void UiTabPanel::selectTab(const string& documentId)
{
    auto it = std::find_if(tabs.begin(),
                           tabs.end(),
                           [&](const UiTab& tab)
                           { return tab.documentId == documentId; });
    if (it != tabs.end())
    {
        if (currentTab != -1)
        {
            userInterface->hideDocument(tabs[currentTab].documentId);
        }
        int index = static_cast<int>(it - tabs.begin());
        if (index == currentTab)
        {
            currentTab = -1;
        }
        else
        {
            currentTab = it - tabs.begin();
            userInterface->showDocument(documentId);
        }
    }
    else
    {
        LG_W("Tab not found: {}", documentId);
    }
}

void UiTabPanel::hideCurrentTab()
{
    if (currentTab != -1)
    {
        userInterface->hideDocument(tabs[currentTab].documentId);
        currentTab = -1;
    }
}

void UiTabPanel::setupDataModel()
{
    auto dataModelConstructor = userInterface->getDataModel(id);
    if (dataModelConstructor)
    {
        if (auto md_handle = dataModelConstructor.RegisterStruct<UiTab>())
        {
            md_handle.RegisterMember("title", &UiTab::title);
            md_handle.RegisterMember("documentId", &UiTab::documentId);
        }
        dataModelConstructor.RegisterArray<std::vector<UiTab>>();
        dataModelConstructor.Bind("tabs", &tabs);
        dataModelConstructor.Bind("currentTab", &currentTab);
        dataModelConstructor.BindEventCallback(
            "onTabSelected", &UiTabPanel::onTabSelected, this);
    }
    dataModelHandle = dataModelConstructor.GetModelHandle();
}

void UiTabPanel::onTabSelected(Rml::DataModelHandle handle,
                               Rml::Event& event,
                               const Rml::VariantList& args)
{
    if (args.size() > 0)
    {
        std::string documentId = args[0].Get<std::string>();
        selectTab(documentId);
    }
}

const string& UiTabPanel::getCurrentTabDocumentId() const
{
    if (currentTab != -1)
    {
        return tabs[currentTab].documentId;
    }
    return emptyString;
}

bool UiTabPanel::hasTab(const string& documentId) const
{
    return std::find_if(tabs.begin(),
                        tabs.end(),
                        [&](const UiTab& tab)
                        { return tab.documentId == documentId; }) != tabs.end();
}

}  // namespace ui