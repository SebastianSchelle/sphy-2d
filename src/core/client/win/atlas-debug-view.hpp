#ifndef ATLAS_DEBUG_VIEW_HPP
#define ATLAS_DEBUG_VIEW_HPP

#include <render/texture.hpp>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Variant.h>
#include <string>
#include <vector>

namespace ui
{
class UserInterface;
}

namespace gfx
{
class RenderEngine;
}

namespace sphyc
{
class Model;
}

namespace ui
{

/// Full-GPU-atlas debug viewer (separate game state). Right-hand sidebar with
/// controls; preview uses the remaining window area at correct aspect ratio.
class AtlasDebugView
{
  public:
    void bind(ui::UserInterface* ui,
              sphyc::Model* model,
              gfx::RenderEngine* renderEngine);

    void setupDataModel(ui::UserInterface& userInterface);
    void openUi(ui::UserInterface& userInterface);
    void closeUi(ui::UserInterface& userInterface, sphyc::Model& model);

    void draw();

    /// Fixed width of the Rml sidebar; must match `style.rcss` (`.atlas-debug-sidebar`).
    static constexpr int kSidebarWidthPx = 300;

    void onKeyPrevMip();
    void onKeyNextMip();

    void onAtlasKindSelect(Rml::DataModelHandle handle,
                           Rml::Event& event,
                           const Rml::VariantList& args);
    void onAtlasPickSelect(Rml::DataModelHandle handle,
                           Rml::Event& event,
                           const Rml::VariantList& args);
    void onAtlasDebugClose(Rml::DataModelHandle handle,
                           Rml::Event& event,
                           const Rml::VariantList& args);

    void refreshAfterGpuArraysChange();

  private:
    void syncLabels();
    void clampIndices();
    void rebuildOptionLists();
    void syncKindIndexFromGpu();
    void refreshSelectionUi();
    void dirtyModel();

    ui::UserInterface* boundUi = nullptr;
    sphyc::Model* boundModel = nullptr;
    gfx::RenderEngine* engine = nullptr;

    Rml::DataModelHandle modelHandle = {};

    int atlasKindIndex = 0;
    int arrayIndex = 0;
    int layerIndex = 0;
    int mipLevel = 0;
    int mipMax = 0;
    int layerMax = 0;

    std::string registrySummary;
    std::string statusLine;
    std::string mipHint;

    std::vector<gfx::AtlasDebugSelectOption> gpuArrayOptions;
    std::vector<gfx::AtlasDebugSelectOption> layerOptions;
    std::vector<gfx::AtlasDebugSelectOption> mipOptions;
    std::vector<gfx::AtlasDebugKindPickRow> atlasKindPickRows;
};

}  // namespace ui

#endif
