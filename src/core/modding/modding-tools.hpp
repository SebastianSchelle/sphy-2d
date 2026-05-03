#ifndef MODDING_TOOLS_HPP
#define MODDING_TOOLS_HPP

#include "std-inc.hpp"
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <lib-textures.hpp>
#include <lib-modules.hpp>

namespace gfx
{
class RenderEngine;
}

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

struct TextureInfo
{
    string name;
    string posX = "0.0";
    string posY = "0.0";
    string sizeX = "100.0";
    string sizeY = "100.0";
    string rot = "0.0";
    string zIndex = "0";

    float posXVal = 0.0f;
    float posYVal = 0.0f;
    float sizeXVal = 100.0f;
    float sizeYVal = 100.0f;
    float rotVal = 0.0f;
    gobj::TextureFlags flags;
    int zIndexVal = 0;
};

struct SlotInfo
{
    string slotType = "RoofS_Common";
    string posX = "0.0";
    string posY = "0.0";
    string rot = "0.0";
    string zIndex = "0";

    gobj::ModuleSlotType slotTypeVal = gobj::ModuleSlotType::RoofS_Common;
    float posXVal = 0.0f;
    float posYVal = 0.0f;
    float rotVal = 0.0f;
    int zIndexVal = 0;
};

struct GeneralInfo
{
    string name = "new";
    string hp = "1000.0";
    string mapIcon = "frigate";
};

class ModdingTools
{
  public:
    ModdingTools() = default;

    void setupDataModel(ui::UserInterface& userInterface);
    void openToolsUi(ui::UserInterface& userInterface);
    void draw(gfx::RenderEngine& renderer);

    Rml::DataModelHandle dataModel() const
    {
        return rmlModel_;
    }

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
    void onAddTexture(Rml::DataModelHandle handle,
                      Rml::Event& event,
                      const Rml::VariantList& args);
    void onClearTextures(Rml::DataModelHandle handle,
                         Rml::Event& event,
                         const Rml::VariantList& args);
    void onAddSlot(Rml::DataModelHandle handle,
                   Rml::Event& event,
                   const Rml::VariantList& args);
    void onClearSlots(Rml::DataModelHandle handle,
                      Rml::Event& event,
                      const Rml::VariantList& args);
    void onRemoveSlot(Rml::DataModelHandle handle,
                      Rml::Event& event,
                      const Rml::VariantList& args);
    void syncModeToRml();

    void drawRoofSlot(gfx::RenderEngine& renderer, const SlotInfo& slot);
    void drawThrusterMainSlot(gfx::RenderEngine& renderer, const SlotInfo& slot);
    void drawThrusterManeuverSlot(gfx::RenderEngine& renderer, const SlotInfo& slot);
    void drawInternalSlot(gfx::RenderEngine& renderer, const SlotInfo& slot);
    void drawBaySlot(gfx::RenderEngine& renderer, const SlotInfo& slot);

    bool saveHullDataToPath(const string& path);
    bool loadHullDataFromPath(const string& path);
    void parseEditorNumericFields();

    Rml::DataModelHandle rmlModel_;
    string openFilepath;
    ModdingToolsMode activeMode = ModdingToolsMode::None;
    /** magic_enum::enum_name(activeMode); bound to Rml as "mode" */
    string mode;
    GeneralInfo hull;
    float hpVal = 0.0f;
    vector<TextureInfo> textures;
    vector<SlotInfo> slots;
    bool extendTextures = true;
    bool extendSlots = true;
};

}  // namespace modding

#endif
