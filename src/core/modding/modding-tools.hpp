#ifndef MODDING_TOOLS_HPP
#define MODDING_TOOLS_HPP

#include "std-inc.hpp"
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <lib-textures.hpp>
#include <lib-modules.hpp>
#include <lib-station-part.hpp>

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

enum class SelectableObjectType
{
    None,
    Texture,
    Slot,
    ColliderVertex,
    Connector,
};

enum class ModdingToolsMode
{
    None,
    Hull,
    Module,
    StationPart,
};

enum class ModdingEditorKey
{
    Copy,
    Paste,
    Delete,
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
    string tileCntX = "1";
    string tileCntY = "1";
    string tileOffX = "0";
    string tileOffY = "0";

    float posXVal = 0.0f;
    float posYVal = 0.0f;
    float sizeXVal = 100.0f;
    float sizeYVal = 100.0f;
    float rotVal = 0.0f;
    float tileCntXVal = 1.0f;
    float tileCntYVal = 1.0f;
    float tileOffXVal = 0.0f;
    float tileOffYVal = 0.0f;
    gobj::TextureFlags flags;
    int zIndexVal = 0;
    /** UI only: name picker list for this row (not saved to YAML). */
    vector<string> nameSuggestions;
    /** UI only: compact tile repeat/offset summary for list rows. */
    string tileModifiers;
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

struct ColliderVertex
{
    string x = "0.0";
    string y = "0.0";
    float xVal = 0.0f;
    float yVal = 0.0f;
};

struct GeneralInfo
{
    string name = "new";
    string hp = "1000.0";
    string mapIcon = "frigate";
    float colliderRestitutionVal = 0.1f;
};

struct StationPartInfo
{
    string partType = "Structural";
    gobj::StationPartType partTypeVal = gobj::StationPartType::Structural;
    /** `data.volume` for Storage parts (see hulls.yaml). */
    string storageVolume = "0";
    float storageVolumeVal = 0.0f;
};

struct ConnectorInfo
{
    string posX = "0.0";
    string posY = "0.0";
    string rot = "0.0";
    float posXVal = 0.0f;
    float posYVal = 0.0f;
    float rotDegVal = 0.0f;
};

class ModdingTools
{
  public:
    ModdingTools() = default;

    void setupDataModel(ui::UserInterface& userInterface);
    void openToolsUi(ui::UserInterface& userInterface);
    void draw(gfx::RenderEngine& renderer);

    void onSingleClick(const glm::vec2& worldPos,
                       float worldZoom,
                       gfx::RenderEngine* renderer = nullptr,
                       bool shiftAlignTextures = false);
    void onLeftMouseDown(const glm::vec2& worldPos,
                         float worldZoom,
                         float dragThresholdWorld,
                         gfx::RenderEngine* renderer = nullptr,
                         bool shiftAlignTextures = false);
    /** While LMB held; uses ui::MouseState::dragActive[0] plus world-space deadzone (same as MouseState). */
    void onLeftMouseDrag(const glm::vec2& worldPos,
                         float worldZoom,
                         bool mouseDragActiveLmb);
    void onLeftMouseUp();

    void onRightMouseDown(const glm::vec2& worldPos, float dragThresholdWorld);
    void onRightMouseDrag(const glm::vec2& worldPos,
                          float worldZoom,
                          bool mouseDragActiveRmb);
    void onRightMouseUp();

    /** Copy/paste/delete when the key was not consumed by RmlUI (e.g. not typing in a field). */
    bool onEditorKey(ModdingEditorKey editorKey);

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
    void onRemoveTexture(Rml::DataModelHandle handle,
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
    void onAddColliderVertex(Rml::DataModelHandle handle,
                             Rml::Event& event,
                             const Rml::VariantList& args);
    void onClearColliderVertices(Rml::DataModelHandle handle,
                                 Rml::Event& event,
                                 const Rml::VariantList& args);
    void onRemoveColliderVertex(Rml::DataModelHandle handle,
                                Rml::Event& event,
                                const Rml::VariantList& args);
    void onAddConnector(Rml::DataModelHandle handle,
                        Rml::Event& event,
                        const Rml::VariantList& args);
    void onClearConnectors(Rml::DataModelHandle handle,
                           Rml::Event& event,
                           const Rml::VariantList& args);
    void onRemoveConnector(Rml::DataModelHandle handle,
                           Rml::Event& event,
                           const Rml::VariantList& args);
    void onTextureNameFocus(Rml::DataModelHandle handle,
                            Rml::Event& event,
                            const Rml::VariantList& args);
    void onGlobalNewTexturePickerFocus(Rml::DataModelHandle handle,
                                       Rml::Event& event,
                                       const Rml::VariantList& args);
    /** Hides per-row name suggestions (focus moved off the name field). */
    void onTextureRowNonNameFocus(Rml::DataModelHandle handle,
                                  Rml::Event& event,
                                  const Rml::VariantList& args);
    void onPickTextureName(Rml::DataModelHandle handle,
                           Rml::Event& event,
                           const Rml::VariantList& args);
    void onPickNewTextureNameFromPicker(Rml::DataModelHandle handle,
                                        Rml::Event& event,
                                        const Rml::VariantList& args);
    void onModdingSelectListRow(Rml::DataModelHandle handle,
                                Rml::Event& event,
                                const Rml::VariantList& args);
    void syncModeToRml();
    void syncListSelectionToRml();
    void fixSelectionAfterErase(SelectableObjectType listKind, int erasedIndex);
    bool canEditObjectType(SelectableObjectType type) const;
    void copySelectedToClipboard();
    bool pasteFromClipboard();
    bool deleteSelectedObject();

    void drawRoofSlot(gfx::RenderEngine& renderer,
                      const SlotInfo& slot,
                      bool selected);
    void drawThrusterMainSlot(gfx::RenderEngine& renderer,
                              const SlotInfo& slot,
                              bool selected);
    void drawThrusterManeuverSlot(gfx::RenderEngine& renderer,
                                 const SlotInfo& slot,
                                 bool selected);
    void drawInternalSlot(gfx::RenderEngine& renderer,
                          const SlotInfo& slot,
                          bool selected);
    void drawBaySlot(gfx::RenderEngine& renderer,
                     const SlotInfo& slot,
                     bool selected);
    void drawColliders(gfx::RenderEngine& renderer);
    void drawSlots(gfx::RenderEngine& renderer);
    void drawTextures(gfx::RenderEngine& renderer);
    void drawConnectors(gfx::RenderEngine& renderer);

    bool saveHullDataToPath(const string& path);
    bool loadHullDataFromPath(const string& path);
    bool saveStationPartDataToPath(const string& path);
    bool loadStationPartDataFromPath(const string& path);
    /** Drops any station-connector rows and appends one per connector (StationPart mode). */
    void syncStationPartConnectorTextures();
    void parseEditorNumericFields();
    void refreshPerRowTextureNameSuggestions();
    void refreshNewTexturePickerSuggestions();
    void syncTextureSizesFromNames(gfx::RenderEngine& renderer);
    void resetNewTexturePickerState();
    TextureInfo makeNewTextureEntry();
    ModdingToolsMode determineAssetType(const string& path);

    Rml::DataModelHandle rmlModel_;
    string openFilepath;
    ModdingToolsMode activeMode = ModdingToolsMode::None;
    /** magic_enum::enum_name(activeMode); bound to Rml as "mode" */
    string mode;
    GeneralInfo genInfo;
    StationPartInfo stationPartInfo;
    float hpVal = 0.0f;
    vector<TextureInfo> textures;
    vector<string> renderTextureNames;
    /** Add-texture row: filter field and suggestion list (Hull / StationPart). */
    string newTexturePickerName;
    vector<string> filteredNewTexturePickerNames;
    /** Last name|tileCnt sync key we applied auto pixel-size scaling for. */
    vector<string> textureSizeAppliedForName;
    vector<SlotInfo> slots;
    vector<ColliderVertex> collider;
    vector<ConnectorInfo> connectors;
    /** Physics restitution for hull collider; YAML key `restitution` under `collider:<hullKey>`. */
    bool extendTextures = true;
    bool extendSlots = true;
    bool extendCollider = true;
    bool extendConnectors = true;
    int activeTextureIndex = -1;
    /** For RML row highlight / expressions (`magic_enum::enum_name`). */
    string selectedListKind = "None";
    int selectedListIndex = -1;
    /** Per-row texture name field focus; controls visibility of that row's name
     * suggestions (-1 = none). Global "Add by name" list is unaffected. */
    int textureRowNameFocusIndex = -1;

    SelectableObjectType selectedObjectType = SelectableObjectType::None;
    int selectedObjectIndex = -1;
    bool dragSelectedObject = false;
    /** Set when shift+texture align runs so LMB drag does not move selection. */
    bool suppressDragAfterClick = false;

    glm::vec2 lmbDragPressWorld{};
    float lmbDragThresholdCfg = 300.f;
    bool lmbPastDragDeadzone = false;

    bool rmbRotateGesture = false;
    glm::vec2 rmbDragPressWorld{};
    glm::vec2 rmbRotatePrevWorld{};
    float rmbDragThresholdCfg = 300.f;
    bool rmbPastDragDeadzone = false;

    struct ModdingClipboard
    {
        bool valid = false;
        SelectableObjectType type = SelectableObjectType::None;
        TextureInfo texture;
        SlotInfo slot;
        ColliderVertex colliderVertex;
        ConnectorInfo connector;
    };
    ModdingClipboard clipboard_;

};

}  // namespace modding

#endif
