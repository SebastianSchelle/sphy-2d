#include "modding-tools.hpp"
#include <magic_enum/magic_enum.hpp>
#include <std-inc.hpp>
#include <ui/user-interface.hpp>

namespace modding
{

void ModdingTools::syncModeToRml()
{
    mode = std::string(magic_enum::enum_name(activeMode));
    if (rmlModel_)
    {
        rmlModel_.DirtyVariable("mode");
    }
}

void ModdingTools::setupDataModel(ui::UserInterface& userInterface)
{
    auto moddingToolsConstructor = userInterface.getDataModel("modding-tools");
    if (!moddingToolsConstructor)
    {
        return;
    }
    LG_D("Data model 'modding-tools' created");
    mode = std::string(magic_enum::enum_name(activeMode));
    moddingToolsConstructor.Bind("mode", &mode);
    moddingToolsConstructor.BindEventCallback(
        "onModdingNewHull", &ModdingTools::onModdingNewHull, this);
    moddingToolsConstructor.BindEventCallback(
        "onModdingNewModule", &ModdingTools::onModdingNewModule, this);
    moddingToolsConstructor.BindEventCallback(
        "onModdingNewStationPart", &ModdingTools::onModdingNewStationPart, this);
    moddingToolsConstructor.BindEventCallback(
        "onModdingFileSave", &ModdingTools::onModdingFileSave, this);
    moddingToolsConstructor.BindEventCallback(
        "onModdingFileLoad", &ModdingTools::onModdingFileLoad, this);
    rmlModel_ = moddingToolsConstructor.GetModelHandle();
}

void ModdingTools::openToolsUi(ui::UserInterface& userInterface)
{
    activeMode = ModdingToolsMode::None;
    syncModeToRml();
    userInterface.hideAllDocuments();
    userInterface.showDocument(
        userInterface.getDocumentHandle("modding-tools-obj"));
    // userInterface.showDocument(
    //     userInterface.getDocumentHandle("modding-tools-menu"));
}

void ModdingTools::onModdingNewHull(Rml::DataModelHandle handle,
                                    Rml::Event& event,
                                    const Rml::VariantList& args)
{
    activeMode = ModdingToolsMode::Hull;
    openFilepath = "";
    syncModeToRml();
    LG_D("Modding tools: new hull");
}

void ModdingTools::onModdingNewModule(Rml::DataModelHandle handle,
                                      Rml::Event& event,
                                      const Rml::VariantList& args)
{
    activeMode = ModdingToolsMode::Module;
    openFilepath = "";
    syncModeToRml();
    LG_D("Modding tools: new module");
}

void ModdingTools::onModdingNewStationPart(Rml::DataModelHandle handle,
                                             Rml::Event& event,
                                             const Rml::VariantList& args)
{
    activeMode = ModdingToolsMode::StationPart;
    openFilepath = "";
    syncModeToRml();
    LG_D("Modding tools: new station part");
}

void ModdingTools::onModdingFileSave(Rml::DataModelHandle handle,
                                     Rml::Event& event,
                                     const Rml::VariantList& args)
{
    LG_D("Modding tools: file save");
}

void ModdingTools::onModdingFileLoad(Rml::DataModelHandle handle,
                                     Rml::Event& event,
                                     const Rml::VariantList& args)
{
    LG_D("Modding tools: file load");
}

}  // namespace modding
