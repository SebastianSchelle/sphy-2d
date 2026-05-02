#ifndef MODDING_TOOLS_HPP
#define MODDING_TOOLS_HPP

#include "std-inc.hpp"
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>

namespace ui
{
class UserInterface;
}

namespace modding
{

enum class ModdingToolsMode
{
    None,
    Hull,
    Module,
    StationPart,
};

class ModdingTools
{
  public:
    ModdingTools() = default;

    void setupDataModel(ui::UserInterface& userInterface);
    void openToolsUi(ui::UserInterface& userInterface);

    Rml::DataModelHandle dataModel() const { return rmlModel_; }

  private:
    void onModdingNewHull(Rml::DataModelHandle handle,
                          Rml::Event& event,
                          const Rml::VariantList& args);
    void onModdingNewModule(Rml::DataModelHandle handle,
                            Rml::Event& event,
                            const Rml::VariantList& args);
    void onModdingNewStationPart(Rml::DataModelHandle handle,
                                 Rml::Event& event,
                                 const Rml::VariantList& args);
    void onModdingFileSave(Rml::DataModelHandle handle,
                           Rml::Event& event,
                           const Rml::VariantList& args);
    void onModdingFileLoad(Rml::DataModelHandle handle,
                           Rml::Event& event,
                           const Rml::VariantList& args);

    void syncModeToRml();

    Rml::DataModelHandle rmlModel_;
    string openFilepath;
    ModdingToolsMode activeMode = ModdingToolsMode::None;
    /** magic_enum::enum_name(activeMode); bound to Rml as "mode" */
    string mode;
};

}  // namespace modding

#endif
