#include "atlas-debug-view.hpp"
#include <bgfx/bgfx.h>
#include <model.hpp>
#include <render/render-engine.hpp>
#include <ui/user-interface.hpp>
#include <algorithm>

namespace ui
{

namespace
{

uint8_t mipCountForSize(uint16_t w, uint16_t h)
{
    uint8_t levels = 1;
    uint32_t mw = w;
    uint32_t mh = h;
    while (mw > 1u || mh > 1u)
    {
        mw = std::max(1u, mw >> 1u);
        mh = std::max(1u, mh >> 1u);
        ++levels;
    }
    return levels;
}

}  // namespace

void AtlasDebugView::bind(ui::UserInterface* ui,
                          sphyc::Model* model,
                          gfx::RenderEngine* renderEngine)
{
    boundUi = ui;
    boundModel = model;
    engine = renderEngine;
}

void AtlasDebugView::setupDataModel(ui::UserInterface& userInterface)
{
    auto ctor = userInterface.getDataModel("atlas-debug");
    if (!ctor)
    {
        return;
    }

    if (auto h = ctor.RegisterStruct<gfx::AtlasDebugSelectOption>())
    {
        h.RegisterMember("value", &gfx::AtlasDebugSelectOption::value);
        h.RegisterMember("label", &gfx::AtlasDebugSelectOption::label);
    }
    ctor.RegisterArray<std::vector<gfx::AtlasDebugSelectOption>>();

    if (auto h = ctor.RegisterStruct<gfx::AtlasDebugKindPickRow>())
    {
        h.RegisterMember("label", &gfx::AtlasDebugKindPickRow::label);
        h.RegisterMember("gpuArrayIndex", &gfx::AtlasDebugKindPickRow::gpuArrayIndex);
        h.RegisterMember("layerIndex", &gfx::AtlasDebugKindPickRow::layerIndex);
    }
    ctor.RegisterArray<std::vector<gfx::AtlasDebugKindPickRow>>();

    ctor.Bind("atlasKindIndex", &atlasKindIndex);
    ctor.Bind("arrayIndex", &arrayIndex);
    ctor.Bind("layerIndex", &layerIndex);
    ctor.Bind("mipLevel", &mipLevel);
    ctor.Bind("layerMax", &layerMax);
    ctor.Bind("mipMax", &mipMax);
    ctor.Bind("registrySummary", &registrySummary);
    ctor.Bind("statusLine", &statusLine);
    ctor.Bind("mipHint", &mipHint);
    ctor.Bind("gpuArrayOptions", &gpuArrayOptions);
    ctor.Bind("layerOptions", &layerOptions);
    ctor.Bind("mipOptions", &mipOptions);
    ctor.Bind("atlasKindPickRows", &atlasKindPickRows);

    ctor.BindEventCallback("onAtlasKindSelect",
                           &AtlasDebugView::onAtlasKindSelect,
                           this);
    ctor.BindEventCallback("onAtlasPickSelect",
                           &AtlasDebugView::onAtlasPickSelect,
                           this);
    ctor.BindEventCallback("onAtlasDebugClose",
                           &AtlasDebugView::onAtlasDebugClose,
                           this);
    modelHandle = ctor.GetModelHandle();
}

void AtlasDebugView::openUi(ui::UserInterface& userInterface)
{
    userInterface.hideAllDocuments();
    userInterface.showDocument(
        userInterface.getDocumentHandle("atlas-debug-menu"));
}

void AtlasDebugView::closeUi(ui::UserInterface& userInterface,
                             sphyc::Model& model)
{
    userInterface.hideDocument(
        userInterface.getDocumentHandle("atlas-debug-menu"));
    model.endAtlasDebug();
    userInterface.closeMenu();
    userInterface.showMenu();
}

void AtlasDebugView::dirtyModel()
{
    if (!modelHandle)
    {
        return;
    }
    modelHandle.DirtyVariable("atlasKindIndex");
    modelHandle.DirtyVariable("arrayIndex");
    modelHandle.DirtyVariable("layerIndex");
    modelHandle.DirtyVariable("mipLevel");
    modelHandle.DirtyVariable("layerMax");
    modelHandle.DirtyVariable("mipMax");
    modelHandle.DirtyVariable("registrySummary");
    modelHandle.DirtyVariable("statusLine");
    modelHandle.DirtyVariable("mipHint");
    modelHandle.DirtyVariable("gpuArrayOptions");
    modelHandle.DirtyVariable("layerOptions");
    modelHandle.DirtyVariable("mipOptions");
    modelHandle.DirtyVariable("atlasKindPickRows");
}

void AtlasDebugView::rebuildOptionLists()
{
    if (!engine)
    {
        return;
    }
    engine->fillAtlasDebugGpuArrayOptions(gpuArrayOptions);
    engine->fillAtlasDebugKindPickRows(atlasKindPickRows);
    engine->fillAtlasDebugLayerOptions(arrayIndex, layerOptions);
    engine->fillAtlasDebugMipOptions(arrayIndex, mipOptions);
    registrySummary = engine->getAtlasRegistrySummary();
}

void AtlasDebugView::syncKindIndexFromGpu()
{
    atlasKindIndex = 0;
    for (size_t i = 1; i < atlasKindPickRows.size(); ++i)
    {
        const gfx::AtlasDebugKindPickRow& r = atlasKindPickRows[i];
        if (r.gpuArrayIndex == arrayIndex && r.layerIndex == layerIndex)
        {
            atlasKindIndex = static_cast<int>(i);
            break;
        }
    }
}

void AtlasDebugView::refreshSelectionUi()
{
    clampIndices();
    rebuildOptionLists();
    clampIndices();
    syncKindIndexFromGpu();
    syncLabels();
    dirtyModel();
}

void AtlasDebugView::clampIndices()
{
    if (!engine)
    {
        return;
    }
    const size_t n = engine->getGpuTextureArrayCount();
    const int arrayCount = static_cast<int>(n);
    if (arrayCount < 1)
    {
        arrayIndex = 0;
        layerIndex = 0;
        layerMax = 0;
        mipLevel = 0;
        mipMax = 0;
        return;
    }
    if (arrayIndex >= arrayCount)
    {
        arrayIndex = arrayCount - 1;
    }
    if (arrayIndex < 0)
    {
        arrayIndex = 0;
    }

    gfx::GpuTextureArrayInfo info{};
    if (!engine->getGpuTextureArrayInfo(static_cast<size_t>(arrayIndex), info))
    {
        layerMax = 0;
        mipMax = 1;
        return;
    }
    layerMax = std::max(0, int(info.layersUsed) - 1);
    if (layerIndex > layerMax)
    {
        layerIndex = layerMax;
    }
    if (layerIndex < 0)
    {
        layerIndex = 0;
    }
    mipMax = int(mipCountForSize(info.width, info.height)) - 1;
    if (mipLevel > mipMax)
    {
        mipLevel = mipMax;
    }
    if (mipLevel < 0)
    {
        mipLevel = 0;
    }
}

void AtlasDebugView::syncLabels()
{
    if (!engine)
    {
        statusLine = "Render engine not bound.";
        mipHint = "";
        return;
    }
    gfx::GpuTextureArrayInfo info{};
    if (!engine->getGpuTextureArrayInfo(static_cast<size_t>(arrayIndex), info))
    {
        statusLine = "No GPU texture arrays.";
        mipHint = "Load a world or assets to populate atlases.";
        return;
    }
    const int ac = static_cast<int>(engine->getGpuTextureArrayCount());
    const int maxArr = std::max(0, ac - 1);
    statusLine = "GPU " + std::to_string(arrayIndex) + " / "
                 + std::to_string(maxArr) + "   "
                 + std::to_string(int(info.width)) + "×"
                 + std::to_string(int(info.height)) + "   layer "
                 + std::to_string(layerIndex) + "/"
                 + std::to_string(int(info.layersUsed ? info.layersUsed - 1
                                                      : 0))
                 + "   cap "
                 + std::to_string(int(info.layersCapacity)) + " layers";
    mipHint = "Mip " + std::to_string(mipLevel) + " / " + std::to_string(mipMax)
              + "   ← / → to change";
}

void AtlasDebugView::refreshAfterGpuArraysChange()
{
    refreshSelectionUi();
}

void AtlasDebugView::draw()
{
    if (!engine)
    {
        return;
    }
    gfx::GpuTextureArrayInfo info{};
    if (!engine->getGpuTextureArrayInfo(static_cast<size_t>(arrayIndex), info)
        || !bgfx::isValid(info.handle))
    {
        return;
    }
    const uint8_t mip = static_cast<uint8_t>(
        std::clamp(mipLevel, 0, std::max(0, mipMax)));
    const uint8_t layer = static_cast<uint8_t>(
        std::clamp(layerIndex, 0, std::max(0, layerMax)));
    const glm::ivec2 winPx = engine->getWindowPixelSize();
    const int previewW = std::max(1, winPx.x - kSidebarWidthPx);
    const int previewH = std::max(1, winPx.y);
    engine->drawAtlasDebugLayer(0,
                                info.handle,
                                layer,
                                mip,
                                info.width,
                                info.height,
                                0,
                                0,
                                previewW,
                                previewH);
}

void AtlasDebugView::onKeyPrevMip()
{
    if (mipLevel > 0)
    {
        --mipLevel;
        syncKindIndexFromGpu();
        syncLabels();
        dirtyModel();
    }
}

void AtlasDebugView::onKeyNextMip()
{
    if (mipLevel < mipMax)
    {
        ++mipLevel;
        syncKindIndexFromGpu();
        syncLabels();
        dirtyModel();
    }
}

void AtlasDebugView::onAtlasKindSelect(Rml::DataModelHandle handle,
                                       Rml::Event& event,
                                       const Rml::VariantList& args)
{
    (void)handle;
    (void)event;
    (void)args;
    if (!engine)
    {
        return;
    }

    if (atlasKindIndex > 0
        && atlasKindIndex < static_cast<int>(atlasKindPickRows.size()))
    {
        const gfx::AtlasDebugKindPickRow& r = atlasKindPickRows[static_cast<size_t>(
            atlasKindIndex)];
        if (r.gpuArrayIndex >= 0)
        {
            arrayIndex = r.gpuArrayIndex;
            layerIndex = r.layerIndex;
        }
    }

    refreshSelectionUi();
}

void AtlasDebugView::onAtlasPickSelect(Rml::DataModelHandle handle,
                                       Rml::Event& event,
                                       const Rml::VariantList& args)
{
    (void)handle;
    (void)event;
    (void)args;
    if (!engine)
    {
        return;
    }
    refreshSelectionUi();
}

void AtlasDebugView::onAtlasDebugClose(Rml::DataModelHandle handle,
                                       Rml::Event& event,
                                       const Rml::VariantList& args)
{
    (void)handle;
    (void)event;
    (void)args;
    if (boundUi && boundModel)
    {
        closeUi(*boundUi, *boundModel);
    }
}

}  // namespace ui
