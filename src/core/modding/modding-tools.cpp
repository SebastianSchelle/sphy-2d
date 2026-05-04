#include "modding-tools.hpp"
#include "texture.hpp"
#include <cctype>
#include <fstream>
#include <magic_enum/magic_enum.hpp>
#include <render-engine.hpp>
#include <save-file-dialog.hpp>
#include <std-inc.hpp>
#include <user-interface.hpp>
#include <yaml-cpp/yaml.h>

namespace modding
{
namespace
{

string sanitizeHullKey(string s)
{
    string o;
    o.reserve(s.size());
    for (char c : s)
    {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')
        {
            o += c;
        }
        else if (!o.empty() && o.back() != '_')
        {
            o += '_';
        }
    }
    while (!o.empty() && o.front() == '_')
    {
        o.erase(o.begin());
    }
    if (o.empty())
    {
        return "hull";
    }
    return o;
}

string toLowerCopy(const string& input)
{
    string out = input;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

}  // namespace

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
        "onModdingNewStationPart",
        &ModdingTools::onModdingNewStationPart,
        this);
    moddingToolsConstructor.BindEventCallback(
        "onModdingFileSave", &ModdingTools::onModdingFileSave, this);
    moddingToolsConstructor.BindEventCallback(
        "onModdingFileLoad", &ModdingTools::onModdingFileLoad, this);
    moddingToolsConstructor.BindEventCallback(
        "onAddTexture", &ModdingTools::onAddTexture, this);
    moddingToolsConstructor.BindEventCallback(
        "onClearTextures", &ModdingTools::onClearTextures, this);
    moddingToolsConstructor.BindEventCallback(
        "removeTexture", &ModdingTools::onRemoveTexture, this);
    moddingToolsConstructor.BindEventCallback(
        "onAddSlot", &ModdingTools::onAddSlot, this);
    moddingToolsConstructor.BindEventCallback(
        "onClearSlots", &ModdingTools::onClearSlots, this);
    moddingToolsConstructor.BindEventCallback(
        "removeSlot", &ModdingTools::onRemoveSlot, this);
    moddingToolsConstructor.BindEventCallback(
        "onAddColliderVertex", &ModdingTools::onAddColliderVertex, this);
    moddingToolsConstructor.BindEventCallback(
        "onClearColliderVertices",
        &ModdingTools::onClearColliderVertices,
        this);
    moddingToolsConstructor.BindEventCallback(
        "onRemoveColliderVertex", &ModdingTools::onRemoveColliderVertex, this);
    moddingToolsConstructor.BindEventCallback(
        "onAddConnector", &ModdingTools::onAddConnector, this);
    moddingToolsConstructor.BindEventCallback(
        "onClearConnectors", &ModdingTools::onClearConnectors, this);
    moddingToolsConstructor.BindEventCallback(
        "onRemoveConnector", &ModdingTools::onRemoveConnector, this);
    moddingToolsConstructor.BindEventCallback(
        "onTextureNameFocus", &ModdingTools::onTextureNameFocus, this);
    moddingToolsConstructor.BindEventCallback(
        "onPickTextureName", &ModdingTools::onPickTextureName, this);
    if (auto hullHandle = moddingToolsConstructor.RegisterStruct<GeneralInfo>())
    {
        hullHandle.RegisterMember("name", &GeneralInfo::name);
        hullHandle.RegisterMember("hp", &GeneralInfo::hp);
        hullHandle.RegisterMember("mapIcon", &GeneralInfo::mapIcon);
        hullHandle.RegisterMember("colliderRestitution",
                                  &GeneralInfo::colliderRestitutionVal);
    }
    moddingToolsConstructor.Bind("hull", &genInfo);
    if (auto stationPartHandle =
            moddingToolsConstructor.RegisterStruct<StationPartInfo>())
    {
        stationPartHandle.RegisterMember("partType",
                                         &StationPartInfo::partType);
        stationPartHandle.RegisterMember("storageVolume",
                                         &StationPartInfo::storageVolume);
    }
    moddingToolsConstructor.Bind("stationPart", &stationPartInfo);
    if (auto textureHandle =
            moddingToolsConstructor.RegisterStruct<TextureInfo>())
    {
        // Use string-backed editable fields to avoid disruptive parse resets
        // while the user is still typing (e.g. "-", "0.").
        textureHandle.RegisterMember("name", &TextureInfo::name);
        textureHandle.RegisterMember("posX", &TextureInfo::posX);
        textureHandle.RegisterMember("posY", &TextureInfo::posY);
        textureHandle.RegisterMember("sizeX", &TextureInfo::sizeX);
        textureHandle.RegisterMember("sizeY", &TextureInfo::sizeY);
        textureHandle.RegisterMember("rot", &TextureInfo::rot);
        textureHandle.RegisterMember("flags", &TextureInfo::flags);
        textureHandle.RegisterMember("zIndex", &TextureInfo::zIndex);
    }
    moddingToolsConstructor.RegisterArray<std::vector<TextureInfo>>();
    moddingToolsConstructor.Bind("textures", &textures);
    moddingToolsConstructor.RegisterArray<std::vector<string>>();
    moddingToolsConstructor.Bind("renderTextureNames", &renderTextureNames);
    moddingToolsConstructor.Bind("activeTextureIndex", &activeTextureIndex);
    moddingToolsConstructor.Bind("showTextureSuggestions",
                                 &showTextureSuggestions);
    moddingToolsConstructor.RegisterArray<std::vector<string>>();
    moddingToolsConstructor.Bind("filteredTextureNames", &filteredTextureNames);
    if (auto slotHandle = moddingToolsConstructor.RegisterStruct<SlotInfo>())
    {
        slotHandle.RegisterMember("slotType", &SlotInfo::slotType);
        slotHandle.RegisterMember("posX", &SlotInfo::posX);
        slotHandle.RegisterMember("posY", &SlotInfo::posY);
        slotHandle.RegisterMember("rot", &SlotInfo::rot);
        slotHandle.RegisterMember("zIndex", &SlotInfo::zIndex);
    }
    moddingToolsConstructor.RegisterArray<std::vector<SlotInfo>>();
    moddingToolsConstructor.Bind("slots", &slots);

    if (auto colliderHandle =
            moddingToolsConstructor.RegisterStruct<ColliderVertex>())
    {
        colliderHandle.RegisterMember("x", &ColliderVertex::x);
        colliderHandle.RegisterMember("y", &ColliderVertex::y);
    }
    moddingToolsConstructor.RegisterArray<std::vector<ColliderVertex>>();
    moddingToolsConstructor.Bind("collider", &collider);
    if (auto connectorHandle =
            moddingToolsConstructor.RegisterStruct<ConnectorInfo>())
    {
        connectorHandle.RegisterMember("posX", &ConnectorInfo::posX);
        connectorHandle.RegisterMember("posY", &ConnectorInfo::posY);
        connectorHandle.RegisterMember("rot", &ConnectorInfo::rot);
    }
    moddingToolsConstructor.RegisterArray<std::vector<ConnectorInfo>>();
    moddingToolsConstructor.Bind("connectors", &connectors);

    moddingToolsConstructor.Bind("openFilepath", &openFilepath);
    moddingToolsConstructor.Bind("extendTextures", &extendTextures);
    moddingToolsConstructor.Bind("extendSlots", &extendSlots);
    moddingToolsConstructor.Bind("extendCollider", &extendCollider);
    moddingToolsConstructor.Bind("extendConnectors", &extendConnectors);

    rmlModel_ = moddingToolsConstructor.GetModelHandle();
}

void ModdingTools::openToolsUi(ui::UserInterface& userInterface)
{
    activeMode = ModdingToolsMode::None;
    syncModeToRml();
    userInterface.hideAllDocuments();
    userInterface.showDocument(
        userInterface.getDocumentHandle("modding-tools-obj"));
    userInterface.showDocument(
        userInterface.getDocumentHandle("modding-tools-menu"));
}

void ModdingTools::onModdingNewHull(Rml::DataModelHandle handle,
                                    Rml::Event& event,
                                    const Rml::VariantList& args)
{
    activeMode = ModdingToolsMode::Hull;
    openFilepath = "";
    genInfo = GeneralInfo{};
    stationPartInfo = StationPartInfo{};
    textures.clear();
    filteredTextureNames.clear();
    activeTextureIndex = -1;
    showTextureSuggestions = false;
    slots.clear();
    collider.clear();
    connectors.clear();
    genInfo.colliderRestitutionVal = 0.1f;
    syncModeToRml();
    handle.DirtyVariable("openFilepath");
    handle.DirtyVariable("hull");
    handle.DirtyVariable("stationPart");
    handle.DirtyVariable("textures");
    handle.DirtyVariable("slots");
    handle.DirtyVariable("collider");
    handle.DirtyVariable("connectors");
    LG_D("Modding tools: new hull");
}

void ModdingTools::onModdingNewModule(Rml::DataModelHandle handle,
                                      Rml::Event& event,
                                      const Rml::VariantList& args)
{
    activeMode = ModdingToolsMode::Module;
    openFilepath = "";
    syncModeToRml();
    handle.DirtyVariable("openFilepath");
    LG_D("Modding tools: new module");
}

void ModdingTools::onModdingNewStationPart(Rml::DataModelHandle handle,
                                           Rml::Event& event,
                                           const Rml::VariantList& args)
{
    activeMode = ModdingToolsMode::StationPart;
    openFilepath = "";
    genInfo = GeneralInfo{};
    genInfo.mapIcon.clear();
    stationPartInfo = StationPartInfo{};
    textures.clear();
    filteredTextureNames.clear();
    activeTextureIndex = -1;
    showTextureSuggestions = false;
    slots.clear();
    collider.clear();
    connectors.clear();
    genInfo.colliderRestitutionVal = 0.1f;
    hpVal = 1000.0f;
    floatToString(hpVal, genInfo.hp, 2);
    syncModeToRml();
    handle.DirtyVariable("openFilepath");
    handle.DirtyVariable("hull");
    handle.DirtyVariable("stationPart");
    handle.DirtyVariable("textures");
    handle.DirtyVariable("collider");
    handle.DirtyVariable("connectors");
    LG_D("Modding tools: new station part");
}

void ModdingTools::onModdingFileSave(Rml::DataModelHandle handle,
                                     Rml::Event& event,
                                     const Rml::VariantList& args)
{
    (void)event;
    (void)args;
    parseEditorNumericFields();

    const string defaultFile =
        sanitizeHullKey(genInfo.name.empty() ? string("asset") : genInfo.name)
        + ".yaml";
    std::string path;
    if (!osh::pick_save_file_path(path, "Save as", defaultFile))
    {
        return;
    }
    switch (activeMode)
    {
        case ModdingToolsMode::Hull:
        {
            if (textures.empty())
            {
                LG_W("Modding tools: no textures to save");
                return;
            }
            if (!saveHullDataToPath(path))
            {
                LG_W("Modding tools: failed to write {}", path);
                return;
            }
            openFilepath = std::move(path);
            handle.DirtyVariable("openFilepath");
            LG_D("Modding tools: saved hull to {}", openFilepath);
        }
        break;
        case ModdingToolsMode::StationPart:
        {
            if (textures.empty())
            {
                LG_W("Modding tools: no textures to save");
                return;
            }
            if (!saveStationPartDataToPath(path))
            {
                LG_W("Modding tools: failed to write {}", path);
                return;
            }
            openFilepath = std::move(path);
            handle.DirtyVariable("openFilepath");
            LG_D("Modding tools: saved station part to {}", openFilepath);
        }
        break;
        default:
            return;
    }
}

void ModdingTools::onModdingFileLoad(Rml::DataModelHandle handle,
                                     Rml::Event& event,
                                     const Rml::VariantList& args)
{
    (void)event;
    (void)args;
    std::string path;
    if (!osh::pick_open_file_path(path, "Open game object YAML", openFilepath))
    {
        return;
    }
    const ModdingToolsMode assetType = determineAssetType(path);
    switch (assetType)
    {
        case ModdingToolsMode::Hull:
            if (!loadHullDataFromPath(path))
            {
                LG_W("Modding tools: failed to load hull from {}", path);
                return;
            }
            LG_D("Modding tools: loaded hull from {}", openFilepath);
            break;
        case ModdingToolsMode::StationPart:
            if (!loadStationPartDataFromPath(path))
            {
                LG_W("Modding tools: failed to load station part from {}",
                     path);
                return;
            }
            LG_D("Modding tools: loaded station part from {}", openFilepath);
            break;
        default:
            LG_W("Modding tools: unsupported asset type: {}", path);
            return;
    }
    openFilepath = std::move(path);
    handle.DirtyVariable("openFilepath");
    handle.DirtyVariable("hull");
    handle.DirtyVariable("textures");
    handle.DirtyVariable("slots");
    handle.DirtyVariable("collider");
    handle.DirtyVariable("connectors");
    handle.DirtyVariable("stationPart");
    handle.DirtyVariable("mode");
}

void ModdingTools::onAddTexture(Rml::DataModelHandle handle,
                                Rml::Event& event,
                                const Rml::VariantList& args)
{
    textures.push_back(TextureInfo{});
    activeTextureIndex = static_cast<int>(textures.size()) - 1;
    showTextureSuggestions = true;
    refreshTextureNameSuggestions();
    rmlModel_.DirtyVariable("textures");
    rmlModel_.DirtyVariable("activeTextureIndex");
    rmlModel_.DirtyVariable("showTextureSuggestions");
    rmlModel_.DirtyVariable("filteredTextureNames");
}

void ModdingTools::onClearTextures(Rml::DataModelHandle handle,
                                   Rml::Event& event,
                                   const Rml::VariantList& args)
{
    textures.clear();
    filteredTextureNames.clear();
    activeTextureIndex = -1;
    showTextureSuggestions = false;
    rmlModel_.DirtyVariable("textures");
    rmlModel_.DirtyVariable("activeTextureIndex");
    rmlModel_.DirtyVariable("showTextureSuggestions");
    rmlModel_.DirtyVariable("filteredTextureNames");
}

void ModdingTools::onRemoveTexture(Rml::DataModelHandle handle,
                                   Rml::Event& event,
                                   const Rml::VariantList& args)
{
    (void)event;
    if (args.size() != 1)
    {
        return;
    }
    const int i = args[0].Get<int>(-1);
    if (i >= 0 && i < static_cast<int>(textures.size()))
    {
        textures.erase(textures.begin() + static_cast<size_t>(i));
        if (activeTextureIndex == i)
        {
            activeTextureIndex = -1;
            showTextureSuggestions = false;
            filteredTextureNames.clear();
        }
        else if (activeTextureIndex > i)
        {
            activeTextureIndex--;
        }
        refreshTextureNameSuggestions();
        handle.DirtyVariable("textures");
        handle.DirtyVariable("activeTextureIndex");
        handle.DirtyVariable("showTextureSuggestions");
        handle.DirtyVariable("filteredTextureNames");
    }
}

void ModdingTools::onTextureNameFocus(Rml::DataModelHandle handle,
                                      Rml::Event& event,
                                      const Rml::VariantList& args)
{
    (void)event;
    if (args.size() != 1)
    {
        return;
    }
    const int i = args[0].Get<int>(-1);
    if (i < 0 || i >= static_cast<int>(textures.size()))
    {
        return;
    }
    activeTextureIndex = i;
    showTextureSuggestions = true;
    refreshTextureNameSuggestions();
    handle.DirtyVariable("activeTextureIndex");
    handle.DirtyVariable("showTextureSuggestions");
    handle.DirtyVariable("filteredTextureNames");
}

void ModdingTools::onPickTextureName(Rml::DataModelHandle handle,
                                     Rml::Event& event,
                                     const Rml::VariantList& args)
{
    (void)event;
    if (args.size() != 1 || activeTextureIndex < 0
        || activeTextureIndex >= static_cast<int>(textures.size()))
    {
        return;
    }
    const string picked = args[0].Get<string>("");
    if (picked.empty())
    {
        return;
    }
    textures[static_cast<size_t>(activeTextureIndex)].name = picked;
    showTextureSuggestions = false;
    filteredTextureNames.clear();
    handle.DirtyVariable("textures");
    handle.DirtyVariable("showTextureSuggestions");
    handle.DirtyVariable("filteredTextureNames");
}

void ModdingTools::onAddSlot(Rml::DataModelHandle handle,
                             Rml::Event& event,
                             const Rml::VariantList& args)
{
    slots.push_back(SlotInfo{});
    rmlModel_.DirtyVariable("slots");
}

void ModdingTools::onClearSlots(Rml::DataModelHandle handle,
                                Rml::Event& event,
                                const Rml::VariantList& args)
{
    slots.clear();
    rmlModel_.DirtyVariable("slots");
}

void ModdingTools::onRemoveSlot(Rml::DataModelHandle handle,
                                Rml::Event& event,
                                const Rml::VariantList& args)
{
    (void)event;
    if (args.size() != 1)
    {
        return;
    }
    const int i = args[0].Get<int>(-1);
    if (i >= 0 && i < static_cast<int>(slots.size()))
    {
        slots.erase(slots.begin() + static_cast<size_t>(i));
        handle.DirtyVariable("slots");
    }
}


void ModdingTools::onAddColliderVertex(Rml::DataModelHandle handle,
                                       Rml::Event& event,
                                       const Rml::VariantList& args)
{
    collider.push_back(ColliderVertex{});
    rmlModel_.DirtyVariable("collider");
}

void ModdingTools::onClearColliderVertices(Rml::DataModelHandle handle,
                                           Rml::Event& event,
                                           const Rml::VariantList& args)
{
    collider.clear();
    rmlModel_.DirtyVariable("collider");
}

void ModdingTools::onRemoveColliderVertex(Rml::DataModelHandle handle,
                                          Rml::Event& event,
                                          const Rml::VariantList& args)
{
    if (args.size() != 1)
    {
        return;
    }
    const int i = args[0].Get<int>(-1);
    if (i >= 0 && i < static_cast<int>(collider.size()))
    {
        collider.erase(collider.begin() + static_cast<size_t>(i));
        handle.DirtyVariable("collider");
    }
}

void ModdingTools::onAddConnector(Rml::DataModelHandle handle,
                                  Rml::Event& event,
                                  const Rml::VariantList& args)
{
    (void)event;
    (void)args;
    connectors.push_back(ConnectorInfo{});
    handle.DirtyVariable("connectors");
    syncStationPartConnectorTextures();
    handle.DirtyVariable("textures");
}

void ModdingTools::onClearConnectors(Rml::DataModelHandle handle,
                                     Rml::Event& event,
                                     const Rml::VariantList& args)
{
    (void)event;
    (void)args;
    connectors.clear();
    handle.DirtyVariable("connectors");
    syncStationPartConnectorTextures();
    handle.DirtyVariable("textures");
}

void ModdingTools::onRemoveConnector(Rml::DataModelHandle handle,
                                     Rml::Event& event,
                                     const Rml::VariantList& args)
{
    (void)event;
    if (args.size() != 1)
    {
        return;
    }
    const int i = args[0].Get<int>(-1);
    if (i >= 0 && i < static_cast<int>(connectors.size()))
    {
        connectors.erase(connectors.begin() + static_cast<size_t>(i));
        handle.DirtyVariable("connectors");
        syncStationPartConnectorTextures();
        handle.DirtyVariable("textures");
    }
}

void ModdingTools::syncStationPartConnectorTextures()
{
    if (activeMode != ModdingToolsMode::StationPart)
    {
        return;
    }
    textures.erase(std::remove_if(textures.begin(),
                                   textures.end(),
                                   [](const TextureInfo& t) {
                                       return toLowerCopy(t.name)
                                           == "station-connector";
                                   }),
                     textures.end());

    int maxZ = 0;
    for (const auto& t : textures)
    {
        maxZ = std::max(maxZ, t.zIndexVal);
    }
    const int connectorZ = textures.empty() ? 41 : maxZ + 1;

    for (const auto& c : connectors)
    {
        const vec2 p(c.posXVal, c.posYVal);
        const float rad = smath::degToRad(c.rotDegVal);
        const vec2 offset =
            -smath::rotateVec2(vec2(0.0f, gobj::kConnectorHeight / 2.0f), rad);
        const vec2 corner = p + offset;
        TextureInfo t;
        t.name = "station-connector";
        t.posXVal = corner.x;
        t.posYVal = corner.y;
        t.sizeXVal = gobj::kConnectorWidth;
        t.sizeYVal = gobj::kConnectorHeight;
        t.rotVal = c.rotDegVal;
        t.zIndexVal = connectorZ;
        t.flags = gobj::TextureFlags::None;
        floatToString(t.posXVal, t.posX, 2);
        floatToString(t.posYVal, t.posY, 2);
        floatToString(t.sizeXVal, t.sizeX, 2);
        floatToString(t.sizeYVal, t.sizeY, 2);
        floatToString(t.rotVal, t.rot, 2);
        intToString(static_cast<int>(t.zIndexVal), t.zIndex);
        textures.push_back(std::move(t));
    }

    if (activeTextureIndex >= static_cast<int>(textures.size()))
    {
        activeTextureIndex =
            textures.empty() ? -1 : static_cast<int>(textures.size()) - 1;
    }
}

void ModdingTools::parseEditorNumericFields()
{
    int val;
    tryParseFloat(genInfo.hp, hpVal);
    floatToString(hpVal, genInfo.hp, 2);

    for (auto& texture : textures)
    {
        tryParseFloat(texture.posX, texture.posXVal);
        tryParseFloat(texture.posY, texture.posYVal);
        tryParseFloat(texture.sizeX, texture.sizeXVal);
        tryParseFloat(texture.sizeY, texture.sizeYVal);
        tryParseFloat(texture.rot, texture.rotVal);
        tryParseInt(texture.zIndex, texture.zIndexVal);
        floatToString(texture.posXVal, texture.posX, 2);
        floatToString(texture.posYVal, texture.posY, 2);
        floatToString(texture.sizeXVal, texture.sizeX, 2);
        floatToString(texture.sizeYVal, texture.sizeY, 2);
        floatToString(texture.rotVal, texture.rot, 2);
        intToString(texture.zIndexVal, texture.zIndex);
        rmlModel_.DirtyVariable("texture");
    }
    for (auto& slot : slots)
    {
        tryParseFloat(slot.posX, slot.posXVal);
        tryParseFloat(slot.posY, slot.posYVal);
        tryParseFloat(slot.rot, slot.rotVal);
        tryParseInt(slot.zIndex, slot.zIndexVal);
        floatToString(slot.posXVal, slot.posX, 2);
        floatToString(slot.posYVal, slot.posY, 2);
        floatToString(slot.rotVal, slot.rot, 2);
        intToString(slot.zIndexVal, slot.zIndex);
        auto slotType =
            magic_enum::enum_cast<gobj::ModuleSlotType>(slot.slotType);
        if (slotType.has_value())
        {
            slot.slotTypeVal = slotType.value();
        }
        rmlModel_.DirtyVariable("slot");
    }
    for (auto& vertex : collider)
    {
        tryParseFloat(vertex.x, vertex.xVal);
        tryParseFloat(vertex.y, vertex.yVal);
        floatToString(vertex.xVal, vertex.x, 2);
        floatToString(vertex.yVal, vertex.y, 2);
        rmlModel_.DirtyVariable("collider");
    }
    auto partType =
        magic_enum::enum_cast<gobj::StationPartType>(stationPartInfo.partType);
    if (partType.has_value())
    {
        stationPartInfo.partTypeVal = partType.value();
    }
    tryParseFloat(stationPartInfo.storageVolume,
                  stationPartInfo.storageVolumeVal);
    floatToString(
        stationPartInfo.storageVolumeVal, stationPartInfo.storageVolume, 2);
    for (auto& connector : connectors)
    {
        tryParseFloat(connector.posX, connector.posXVal);
        tryParseFloat(connector.posY, connector.posYVal);
        tryParseFloat(connector.rot, connector.rotDegVal);
        floatToString(connector.posXVal, connector.posX, 2);
        floatToString(connector.posYVal, connector.posY, 2);
        floatToString(connector.rotDegVal, connector.rot, 2);
    }
    rmlModel_.DirtyVariable("connectors");
    rmlModel_.DirtyVariable("stationPart");
    rmlModel_.DirtyVariable("hull");
    if (activeMode == ModdingToolsMode::StationPart)
    {
        syncStationPartConnectorTextures();
        rmlModel_.DirtyVariable("textures");
    }
}

void ModdingTools::refreshTextureNameSuggestions()
{
    if (!showTextureSuggestions || activeTextureIndex < 0
        || activeTextureIndex >= static_cast<int>(textures.size()))
    {
        if (!filteredTextureNames.empty())
        {
            filteredTextureNames.clear();
            rmlModel_.DirtyVariable("filteredTextureNames");
        }
        return;
    }

    const string query =
        toLowerCopy(textures[static_cast<size_t>(activeTextureIndex)].name);
    std::vector<string> filtered;
    filtered.reserve(renderTextureNames.size());
    for (const string& textureName : renderTextureNames)
    {
        const string lowerName = toLowerCopy(textureName);
        if (query.empty() || lowerName.find(query) != string::npos)
        {
            filtered.push_back(textureName);
        }
    }
    if (filtered.size() > 20)
    {
        filtered.resize(20);
    }
    if (filteredTextureNames != filtered)
    {
        filteredTextureNames = std::move(filtered);
        rmlModel_.DirtyVariable("filteredTextureNames");
    }
}

void ModdingTools::draw(gfx::RenderEngine& renderer)
{
    const std::vector<string> currentTextureNames = renderer.getTextureNames();
    if (renderTextureNames != currentTextureNames)
    {
        renderTextureNames = currentTextureNames;
        rmlModel_.DirtyVariable("renderTextureNames");
    }
    refreshTextureNameSuggestions();

    parseEditorNumericFields();

    const gfx::ShaderHandle blueprintGrid =
        renderer.getShaderHandle("blueprintgrid");
    if (blueprintGrid.isValid())
    {
        constexpr float kGridCellWorld = 10.0f;
        constexpr float kMajorEveryCells = 5.0f;
        renderer.drawBlueprintGridBackground(
            0, blueprintGrid, kGridCellWorld, kMajorEveryCells);
    }
    switch (activeMode)
    {
        case ModdingToolsMode::Hull:
            drawTextures(renderer);
            drawSlots(renderer);
            drawColliders(renderer);
            break;
        case ModdingToolsMode::StationPart:
            drawTextures(renderer);
            drawColliders(renderer);
            drawConnectors(renderer);
            break;
        default:
            // No drawing for other modes
            break;
    }
}

void ModdingTools::drawTextures(gfx::RenderEngine& renderer)
{
    for (auto& texture : textures)
    {
        const gfx::TextureHandle texHandle =
            renderer.getTextureHandle(texture.name);
        renderer.drawTexRect(glm::vec2(texture.posXVal, texture.posYVal),
                             glm::vec2(texture.sizeXVal, texture.sizeYVal),
                             texHandle,
                             smath::degToRad(texture.rotVal),
                             0xffffffff,
                             texture.zIndexVal / 100.0f,
                             0);
    }
}

void ModdingTools::drawSlots(gfx::RenderEngine& renderer)
{
    for (auto& slot : slots)
    {
        switch (slot.slotTypeVal)
        {
            case gobj::ModuleSlotType::RoofS_Common:
            case gobj::ModuleSlotType::RoofM_Common:
            case gobj::ModuleSlotType::RoofL_Common:
                drawRoofSlot(renderer, slot);
                break;
            case gobj::ModuleSlotType::ThrusterMainS_Common:
            case gobj::ModuleSlotType::ThrusterMainM_Common:
            case gobj::ModuleSlotType::ThrusterMainL_Common:
                drawThrusterMainSlot(renderer, slot);
                break;
            case gobj::ModuleSlotType::ThrusterManeuverS_Common:
            case gobj::ModuleSlotType::ThrusterManeuverM_Common:
            case gobj::ModuleSlotType::ThrusterManeuverL_Common:
                drawThrusterManeuverSlot(renderer, slot);
                break;
            case gobj::ModuleSlotType::InternalS_Common:
            case gobj::ModuleSlotType::InternalM_Common:
            case gobj::ModuleSlotType::InternalL_Common:
                drawInternalSlot(renderer, slot);
                break;
            case gobj::ModuleSlotType::BayS_Common:
            case gobj::ModuleSlotType::BayM_Common:
            case gobj::ModuleSlotType::BayL_Common:
                drawBaySlot(renderer, slot);
                break;
            default:
                break;
        }
    }
}

void ModdingTools::drawColliders(gfx::RenderEngine& renderer)
{
    const float zoom = renderer.getWorldZoom();
    const glm::vec2 dotRadius(3.0f / zoom, 3.0f / zoom);
    const float lineWidth = 1.0f / zoom;

    const ColliderVertex* lastVertex = nullptr;
    if(collider.size() >= 2)
    {
        lastVertex = &collider.back();
    }
    for (const auto& vertex : collider)
    {
        renderer.drawEllipse(glm::vec2(vertex.xVal, vertex.yVal),
                             dotRadius,
                             0xffffffff,
                             0.0f,
                             0.0f,
                             0.0f,
                             0);
        if (lastVertex != nullptr)
        {
            renderer.drawLine(glm::vec2(lastVertex->xVal, lastVertex->yVal),
                              glm::vec2(vertex.xVal, vertex.yVal),
                              0xffffffff,
                              lineWidth,
                              0.0f,
                              0);
        }
        lastVertex = &vertex;
    }
}

void ModdingTools::drawConnectors(gfx::RenderEngine& renderer)
{
    const float zoom = renderer.getWorldZoom();
    const glm::vec2 dotR(4.0f / zoom, 4.0f / zoom);
    const float lineW = 1.0f / zoom;
    for (const auto& c : connectors)
    {
        const glm::vec2 p(c.posXVal, c.posYVal);
        renderer.drawEllipse(p, dotR, 0xffffff00, 0.0f, 0.0f, 0.0f, 0);

        const float rad = smath::degToRad(c.rotDegVal);
        const vec2 offset =
            -smath::rotateVec2(vec2(0.0f, gobj::kConnectorHeight / 2.0f), rad);
        renderer.drawLine(
            p, p - 5.0f * offset, 0xffffff00, lineW, 0.0f, 0);
    }
}

void ModdingTools::drawRoofSlot(gfx::RenderEngine& renderer,
                                const SlotInfo& slot)
{
    float rot = smath::degToRad(slot.rotVal);
    const float s = gobj::SlotSize[static_cast<size_t>(slot.slotTypeVal)];
    renderer.drawEllipse(glm::vec2(slot.posXVal, slot.posYVal),
                         glm::vec2(s, s),
                         0x800000ff,
                         0.0f,
                         rot,
                         slot.zIndexVal / 100.0f,
                         0);
    vec2 offs = smath::rotateVec2(glm::vec2(0.0f, s), rot);
    renderer.drawRectangle(glm::vec2(slot.posXVal, slot.posYVal) + offs,
                           glm::vec2(s * 0.3f, s),
                           0x800000ff,
                           0.0f,
                           rot,
                           slot.zIndexVal / 100.0f,
                           0);
}

void ModdingTools::drawThrusterMainSlot(gfx::RenderEngine& renderer,
                                        const SlotInfo& slot)
{
    const float s = gobj::SlotSize[static_cast<size_t>(slot.slotTypeVal)];
    float rot = smath::degToRad(slot.rotVal);
    renderer.drawRectangle(glm::vec2(slot.posXVal, slot.posYVal),
                           glm::vec2(s, s * 2.0f),
                           0xff100000,
                           0.0f,
                           rot,
                           slot.zIndexVal / 100.0f,
                           0);
    vec2 offs = smath::rotateVec2(glm::vec2(0.0f, s * 2.0f), rot);
    renderer.drawRectangle(glm::vec2(slot.posXVal, slot.posYVal) - offs,
                           glm::vec2(s * 0.6f, s * 2.0f),
                           0x300000ff,
                           0.0f,
                           rot,
                           slot.zIndexVal / 100.0f,
                           0);
}

void ModdingTools::drawThrusterManeuverSlot(gfx::RenderEngine& renderer,
                                            const SlotInfo& slot)
{
    const float s = gobj::SlotSize[static_cast<size_t>(slot.slotTypeVal)];
    float rot = smath::degToRad(slot.rotVal);
    renderer.drawRectangle(glm::vec2(slot.posXVal, slot.posYVal),
                           glm::vec2(s, s * 0.5f),
                           0xff100000,
                           0.0f,
                           rot,
                           slot.zIndexVal / 100.0f,
                           0);
    vec2 offs = smath::rotateVec2(glm::vec2(0.0f, s * 0.5f), rot);
    renderer.drawRectangle(glm::vec2(slot.posXVal, slot.posYVal) - offs,
                           glm::vec2(s * 0.6f, s * 0.5f),
                           0x300000ff,
                           0.0f,
                           rot,
                           slot.zIndexVal / 100.0f,
                           0);
}

void ModdingTools::drawInternalSlot(gfx::RenderEngine& renderer,
                                    const SlotInfo& slot)
{
    const float s = gobj::SlotSize[static_cast<size_t>(slot.slotTypeVal)];
    float rot = smath::degToRad(slot.rotVal);
    renderer.drawRectangle(glm::vec2(slot.posXVal, slot.posYVal),
                           glm::vec2(s, s),
                           0xff44ff44,
                           0.0f,
                           rot,
                           slot.zIndexVal / 100.0f,
                           0);
}

void ModdingTools::drawBaySlot(gfx::RenderEngine& renderer,
                               const SlotInfo& slot)
{
    const float s = gobj::SlotSize[static_cast<size_t>(slot.slotTypeVal)];
    float rot = smath::degToRad(slot.rotVal);
    renderer.drawRectangle(glm::vec2(slot.posXVal, slot.posYVal),
                           glm::vec2(s, s * 0.2f),
                           0xffff8800,
                           0.0f,
                           rot,
                           slot.zIndexVal / 100.0f,
                           0);
}

bool ModdingTools::loadHullDataFromPath(const string& path)
{
    YAML::Node root;
    try
    {
        root = YAML::LoadFile(path);
    }
    catch (const YAML::Exception& e)
    {
        LG_W("Modding tools: YAML error loading {}: {}", path, e.what());
        return false;
    }

    const YAML::Node hullMap = root["hull"];
    if (!hullMap || !hullMap.IsMap() || hullMap.size() == 0)
    {
        LG_W("Modding tools: {} has no hull map", path);
        return false;
    }

    const auto hullIt = hullMap.begin();
    const YAML::Node hullNode = hullIt->second;
    if (!hullNode || !hullNode.IsMap())
    {
        LG_W("Modding tools: invalid hull entry in {}", path);
        return false;
    }

    activeMode = ModdingToolsMode::Hull;
    syncModeToRml();
    textures.clear();
    slots.clear();
    collider.clear();
    connectors.clear();
    stationPartInfo = StationPartInfo{};
    genInfo.colliderRestitutionVal = 0.1f;
    genInfo = GeneralInfo{};

    const string hullMapKey = hullIt->first.as<string>();

    try
    {
        if (hullNode["name"])
        {
            genInfo.name = hullNode["name"].as<string>();
        }
        if (genInfo.name.empty())
        {
            genInfo.name = hullIt->first.as<string>();
        }
        if (hullNode["hullpoints"])
        {
            hpVal = hullNode["hullpoints"].as<float>();
            floatToString(hpVal, genInfo.hp, 2);
        }
        if (hullNode["map-icon"])
        {
            genInfo.mapIcon = hullNode["map-icon"].as<string>();
        }

        string texKey;
        if (hullNode["textures"])
        {
            texKey = hullNode["textures"].as<string>();
        }
        const YAML::Node texRoot = root["textures"];
        if (!texKey.empty() && texRoot && texRoot[texKey]
            && texRoot[texKey]["textures"])
        {
            for (const auto& texNode : texRoot[texKey]["textures"])
            {
                TextureInfo t;
                if (texNode["name"])
                {
                    t.name = texNode["name"].as<string>();
                }
                float px = 0.0f;
                float py = 0.0f;
                float sx = 100.0f;
                float sy = 100.0f;
                const YAML::Node b = texNode["bounds"];
                if (b && b.IsSequence() && b.size() >= 4)
                {
                    px = b[0].as<float>();
                    py = b[1].as<float>();
                    sx = b[2].as<float>();
                    sy = b[3].as<float>();
                }
                t.posXVal = px;
                t.posYVal = py;
                t.sizeXVal = sx;
                t.sizeYVal = sy;
                floatToString(t.posXVal, t.posX, 2);
                floatToString(t.posYVal, t.posY, 2);
                floatToString(t.sizeXVal, t.sizeX, 2);
                floatToString(t.sizeYVal, t.sizeY, 2);

                float rotDeg = 0.0f;
                if (texNode["rot"])
                {
                    rotDeg = texNode["rot"].as<float>();
                }
                t.rotVal = rotDeg;
                floatToString(t.rotVal, t.rot, 2);

                int z = 0;
                if (texNode["zIndex"])
                {
                    z = texNode["zIndex"].as<int>();
                }
                t.zIndexVal = static_cast<int8_t>(z);
                intToString(static_cast<int>(t.zIndexVal), t.zIndex);

                int flagsInt = 0;
                if (texNode["flags"])
                {
                    flagsInt = texNode["flags"].as<int>();
                }
                t.flags = static_cast<gobj::TextureFlags>(flagsInt);

                textures.push_back(std::move(t));
            }
        }
        else if (!texKey.empty())
        {
            LG_W(
                "Modding tools: hull references textures '{}' but bundle "
                "missing in {}",
                texKey,
                path);
        }

        if (hullNode["slots"] && hullNode["slots"].IsSequence())
        {
            for (const auto& sn : hullNode["slots"])
            {
                SlotInfo s;
                string modType = "RoofS_Common";
                if (sn["mod-type"])
                {
                    modType = sn["mod-type"].as<string>();
                }
                s.slotType = modType;
                auto st =
                    magic_enum::enum_cast<gobj::ModuleSlotType>(s.slotType);
                if (st.has_value())
                {
                    s.slotTypeVal = st.value();
                }
                else
                {
                    s.slotType = "RoofS_Common";
                    s.slotTypeVal = gobj::ModuleSlotType::RoofS_Common;
                }

                float px = 0.0f;
                float py = 0.0f;
                const YAML::Node p = sn["pos"];
                if (p && p.IsSequence() && p.size() >= 2)
                {
                    px = p[0].as<float>();
                    py = p[1].as<float>();
                }
                s.posXVal = px;
                s.posYVal = py;
                floatToString(s.posXVal, s.posX, 2);
                floatToString(s.posYVal, s.posY, 2);

                float rotDeg = 0.0f;
                if (sn["rot"])
                {
                    rotDeg = sn["rot"].as<float>();
                }
                s.rotVal = rotDeg;
                floatToString(s.rotVal, s.rot, 2);

                int z = 0;
                if (sn["z"])
                {
                    z = sn["z"].as<int>();
                }
                s.zIndexVal = static_cast<int8_t>(z);
                intToString(static_cast<int>(s.zIndexVal), s.zIndex);

                slots.push_back(std::move(s));
            }
        }

        const YAML::Node colliderRoot = root["collider"];
        if (colliderRoot && colliderRoot[hullMapKey]
            && colliderRoot[hullMapKey].IsMap())
        {
            const YAML::Node colEntry = colliderRoot[hullMapKey];
            if (colEntry["restitution"])
            {
                genInfo.colliderRestitutionVal =
                    colEntry["restitution"].as<float>();
            }
            const YAML::Node vertNode = colEntry["vertices"];
            if (vertNode && vertNode.IsSequence())
            {
                for (const YAML::Node& vn : vertNode)
                {
                    if (!vn.IsSequence() || vn.size() < 2)
                    {
                        continue;
                    }
                    ColliderVertex v;
                    v.xVal = vn[0].as<float>();
                    v.yVal = vn[1].as<float>();
                    floatToString(v.xVal, v.x, 2);
                    floatToString(v.yVal, v.y, 2);
                    collider.push_back(std::move(v));
                }
            }
        }
    }
    catch (const YAML::Exception& e)
    {
        LG_W(
            "Modding tools: error parsing hull data in {}: {}", path, e.what());
        textures.clear();
        slots.clear();
        collider.clear();
        connectors.clear();
        stationPartInfo = StationPartInfo{};
        genInfo.colliderRestitutionVal = 0.1f;
        genInfo = GeneralInfo{};
        activeMode = ModdingToolsMode::None;
        syncModeToRml();
        return false;
    }

    parseEditorNumericFields();
    return true;
}

ModdingToolsMode ModdingTools::determineAssetType(const string& path)
{
    YAML::Node root;
    try
    {
        root = YAML::LoadFile(path);
    }
    catch (const YAML::Exception&)
    {
        return ModdingToolsMode::None;
    }
    if (!root || !root.IsMap())
    {
        return ModdingToolsMode::None;
    }
    const YAML::Node hullMap = root["hull"];
    if (hullMap && hullMap.IsMap() && hullMap.size() > 0)
    {
        return ModdingToolsMode::Hull;
    }
    const YAML::Node spMap = root["station-part"];
    if (spMap && spMap.IsMap() && spMap.size() > 0)
    {
        return ModdingToolsMode::StationPart;
    }
    return ModdingToolsMode::None;
}

bool ModdingTools::loadStationPartDataFromPath(const string& path)
{
    YAML::Node root;
    try
    {
        root = YAML::LoadFile(path);
    }
    catch (const YAML::Exception& e)
    {
        LG_W("Modding tools: YAML error loading {}: {}", path, e.what());
        return false;
    }

    const YAML::Node spMap = root["station-part"];
    if (!spMap || !spMap.IsMap() || spMap.size() == 0)
    {
        LG_W("Modding tools: {} has no station-part map", path);
        return false;
    }

    const auto spIt = spMap.begin();
    const YAML::Node partNode = spIt->second;
    if (!partNode || !partNode.IsMap())
    {
        LG_W("Modding tools: invalid station-part entry in {}", path);
        return false;
    }

    activeMode = ModdingToolsMode::StationPart;
    syncModeToRml();
    textures.clear();
    slots.clear();
    collider.clear();
    connectors.clear();
    stationPartInfo = StationPartInfo{};
    genInfo = GeneralInfo{};
    genInfo.mapIcon.clear();
    genInfo.colliderRestitutionVal = 0.1f;

    const string partMapKey = spIt->first.as<string>();

    try
    {
        if (partNode["name"])
        {
            genInfo.name = partNode["name"].as<string>();
        }
        if (genInfo.name.empty())
        {
            genInfo.name = partMapKey;
        }
        if (partNode["hp"])
        {
            hpVal = partNode["hp"].as<float>();
            floatToString(hpVal, genInfo.hp, 2);
        }
        string typeStr = "Structural";
        if (partNode["type"])
        {
            typeStr = partNode["type"].as<string>();
        }
        stationPartInfo.partType = typeStr;
        const auto pt = magic_enum::enum_cast<gobj::StationPartType>(
            stationPartInfo.partType);
        if (pt.has_value())
        {
            stationPartInfo.partTypeVal = pt.value();
        }
        else
        {
            stationPartInfo.partType = "Structural";
            stationPartInfo.partTypeVal = gobj::StationPartType::Structural;
        }

        stationPartInfo.storageVolumeVal = 0.0f;
        stationPartInfo.storageVolume = "0";
        const YAML::Node dataNode = partNode["data"];
        if (dataNode && dataNode.IsMap() && dataNode["volume"])
        {
            stationPartInfo.storageVolumeVal = dataNode["volume"].as<float>();
            floatToString(stationPartInfo.storageVolumeVal,
                          stationPartInfo.storageVolume,
                          2);
        }

        string texKey;
        if (partNode["textures"])
        {
            texKey = partNode["textures"].as<string>();
        }
        string colKey;
        if (partNode["collider"])
        {
            colKey = partNode["collider"].as<string>();
        }
        if (colKey.empty())
        {
            colKey = partMapKey;
        }

        const YAML::Node texRoot = root["textures"];
        if (!texKey.empty() && texRoot && texRoot[texKey]
            && texRoot[texKey]["textures"])
        {
            for (const auto& texNode : texRoot[texKey]["textures"])
            {
                TextureInfo t;
                if (texNode["name"])
                {
                    t.name = texNode["name"].as<string>();
                }
                if (toLowerCopy(t.name) == "station-connector")
                {
                    continue;
                }
                float px = 0.0f;
                float py = 0.0f;
                float sx = 100.0f;
                float sy = 100.0f;
                const YAML::Node b = texNode["bounds"];
                if (b && b.IsSequence() && b.size() >= 4)
                {
                    px = b[0].as<float>();
                    py = b[1].as<float>();
                    sx = b[2].as<float>();
                    sy = b[3].as<float>();
                }
                t.posXVal = px;
                t.posYVal = py;
                t.sizeXVal = sx;
                t.sizeYVal = sy;
                floatToString(t.posXVal, t.posX, 2);
                floatToString(t.posYVal, t.posY, 2);
                floatToString(t.sizeXVal, t.sizeX, 2);
                floatToString(t.sizeYVal, t.sizeY, 2);

                float rotDeg = 0.0f;
                if (texNode["rot"])
                {
                    rotDeg = texNode["rot"].as<float>();
                }
                t.rotVal = rotDeg;
                floatToString(t.rotVal, t.rot, 2);

                int z = 0;
                if (texNode["zIndex"])
                {
                    z = texNode["zIndex"].as<int>();
                }
                t.zIndexVal = static_cast<int8_t>(z);
                intToString(static_cast<int>(t.zIndexVal), t.zIndex);

                int flagsInt = 0;
                if (texNode["flags"])
                {
                    flagsInt = texNode["flags"].as<int>();
                }
                t.flags = static_cast<gobj::TextureFlags>(flagsInt);

                textures.push_back(std::move(t));
            }
        }
        else if (!texKey.empty())
        {
            LG_W(
                "Modding tools: station-part references textures '{}' but "
                "bundle "
                "missing in {}",
                texKey,
                path);
        }

        const YAML::Node colliderRoot = root["collider"];
        if (colliderRoot && colliderRoot[colKey]
            && colliderRoot[colKey].IsMap())
        {
            const YAML::Node colEntry = colliderRoot[colKey];
            if (colEntry["restitution"])
            {
                genInfo.colliderRestitutionVal =
                    colEntry["restitution"].as<float>();
            }
            const YAML::Node vertNode = colEntry["vertices"];
            if (vertNode && vertNode.IsSequence())
            {
                for (const YAML::Node& vn : vertNode)
                {
                    if (!vn.IsSequence() || vn.size() < 2)
                    {
                        continue;
                    }
                    ColliderVertex v;
                    v.xVal = vn[0].as<float>();
                    v.yVal = vn[1].as<float>();
                    floatToString(v.xVal, v.x, 2);
                    floatToString(v.yVal, v.y, 2);
                    collider.push_back(std::move(v));
                }
            }
        }

        if (partNode["connectors"] && partNode["connectors"].IsSequence())
        {
            for (const auto& cn : partNode["connectors"])
            {
                ConnectorInfo c;
                float px = 0.0f;
                float py = 0.0f;
                const YAML::Node p = cn["pos"];
                if (p && p.IsSequence() && p.size() >= 2)
                {
                    px = p[0].as<float>();
                    py = p[1].as<float>();
                }
                c.posXVal = px;
                c.posYVal = py;
                floatToString(c.posXVal, c.posX, 2);
                floatToString(c.posYVal, c.posY, 2);

                float rotDeg = 0.0f;
                if (cn["rot"])
                {
                    rotDeg = cn["rot"].as<float>();
                }
                c.rotDegVal = rotDeg;
                floatToString(c.rotDegVal, c.rot, 2);

                connectors.push_back(std::move(c));
            }
        }
    }
    catch (const YAML::Exception& e)
    {
        LG_W("Modding tools: error parsing station-part in {}: {}",
             path,
             e.what());
        textures.clear();
        collider.clear();
        connectors.clear();
        stationPartInfo = StationPartInfo{};
        genInfo = GeneralInfo{};
        activeMode = ModdingToolsMode::None;
        syncModeToRml();
        return false;
    }

    parseEditorNumericFields();
    return true;
}

bool ModdingTools::saveStationPartDataToPath(const string& path)
{
    parseEditorNumericFields();

    const string key =
        sanitizeHullKey(genInfo.name.empty() ? string("part") : genInfo.name);
    const string displayName = genInfo.name.empty() ? key : genInfo.name;

    YAML::Node texBundle;
    YAML::Node texList(YAML::NodeType::Sequence);
    for (const auto& t : textures)
    {
        YAML::Node entry;
        entry["name"] = t.name;
        entry["bounds"] = YAML::Node(YAML::NodeType::Sequence);
        entry["bounds"].push_back(t.posXVal);
        entry["bounds"].push_back(t.posYVal);
        entry["bounds"].push_back(t.sizeXVal);
        entry["bounds"].push_back(t.sizeYVal);
        entry["zIndex"] = t.zIndexVal;
        entry["flags"] = static_cast<int>(t.flags);
        entry["rot"] = t.rotVal;
        texList.push_back(entry);
    }
    texBundle["textures"] = texList;

    YAML::Node colEntry;
    if (!collider.empty())
    {
        colEntry["restitution"] = genInfo.colliderRestitutionVal;
        YAML::Node vertSeq(YAML::NodeType::Sequence);
        for (const auto& v : collider)
        {
            YAML::Node pair(YAML::NodeType::Sequence);
            pair.push_back(v.xVal);
            pair.push_back(v.yVal);
            vertSeq.push_back(pair);
        }
        colEntry["vertices"] = vertSeq;
    }

    YAML::Node partNode;
    partNode["name"] = displayName;
    partNode["type"] = stationPartInfo.partType;
    partNode["textures"] = key;
    partNode["collider"] = key;
    partNode["hp"] = hpVal;

    YAML::Node connSeq(YAML::NodeType::Sequence);
    for (const auto& c : connectors)
    {
        YAML::Node cn;
        cn["pos"] = YAML::Node(YAML::NodeType::Sequence);
        cn["pos"].push_back(c.posXVal);
        cn["pos"].push_back(c.posYVal);
        cn["rot"] = c.rotDegVal;
        connSeq.push_back(cn);
    }
    if (!connectors.empty())
    {
        partNode["connectors"] = connSeq;
    }

    YAML::Node dataNode(YAML::NodeType::Map);
    switch (stationPartInfo.partTypeVal)
    {
        case gobj::StationPartType::Structural:
            dataNode["dummy"] = std::string("nodata");
            break;
        case gobj::StationPartType::Storage:
            dataNode["volume"] = stationPartInfo.storageVolumeVal;
            break;
        default:
            break;
    }
    if (dataNode.size() > 0)
    {
        partNode["data"] = dataNode;
    }

    YAML::Node root;
    root["textures"] = YAML::Node(YAML::NodeType::Map);
    root["textures"][key] = texBundle;
    root["collider"] = YAML::Node(YAML::NodeType::Map);
    root["collider"][key] = colEntry;
    root["station-part"] = YAML::Node(YAML::NodeType::Map);
    root["station-part"][key] = partNode;

    std::ofstream out(path);
    if (!out)
    {
        return false;
    }
    out << root;
    return out.good();
}

bool ModdingTools::saveHullDataToPath(const string& path)
{
    parseEditorNumericFields();

    const string key =
        sanitizeHullKey(genInfo.name.empty() ? string("hull") : genInfo.name);
    const string displayName = genInfo.name.empty() ? key : genInfo.name;

    YAML::Node texBundle;
    YAML::Node texList(YAML::NodeType::Sequence);
    for (const auto& t : textures)
    {
        YAML::Node entry;
        entry["name"] = t.name;
        entry["bounds"] = YAML::Node(YAML::NodeType::Sequence);
        entry["bounds"].push_back(t.posXVal);
        entry["bounds"].push_back(t.posYVal);
        entry["bounds"].push_back(t.sizeXVal);
        entry["bounds"].push_back(t.sizeYVal);
        entry["zIndex"] = t.zIndexVal;
        entry["flags"] = static_cast<int>(t.flags);
        entry["rot"] = t.rotVal;
        texList.push_back(entry);
    }
    texBundle["textures"] = texList;

    YAML::Node hullNode;
    hullNode["name"] = displayName;
    hullNode["hullpoints"] = hpVal;
    hullNode["collider"] = key;
    hullNode["textures"] = key;
    if (!genInfo.mapIcon.empty())
    {
        hullNode["map-icon"] = genInfo.mapIcon;
    }

    YAML::Node slotSeq(YAML::NodeType::Sequence);
    for (const auto& s : slots)
    {
        YAML::Node sn;
        sn["mod-type"] = s.slotType;
        sn["pos"] = YAML::Node(YAML::NodeType::Sequence);
        sn["pos"].push_back(s.posXVal);
        sn["pos"].push_back(s.posYVal);
        sn["rot"] = s.rotVal;
        sn["z"] = static_cast<int>(s.zIndexVal);
        slotSeq.push_back(sn);
    }
    if (!slots.empty())
    {
        hullNode["slots"] = slotSeq;
    }

    YAML::Node colEntry;
    if (!collider.empty())
    {
        colEntry["restitution"] = genInfo.colliderRestitutionVal;
        YAML::Node vertSeq(YAML::NodeType::Sequence);
        for (const auto& v : collider)
        {
            YAML::Node pair(YAML::NodeType::Sequence);
            pair.push_back(v.xVal);
            pair.push_back(v.yVal);
            vertSeq.push_back(pair);
        }
        colEntry["vertices"] = vertSeq;
    }

    YAML::Node root;
    root["textures"] = YAML::Node(YAML::NodeType::Map);
    root["textures"][key] = texBundle;
    root["collider"] = YAML::Node(YAML::NodeType::Map);
    root["collider"][key] = colEntry;
    root["hull"] = YAML::Node(YAML::NodeType::Map);
    root["hull"][key] = hullNode;

    std::ofstream out(path);
    if (!out)
    {
        return false;
    }
    out << root;
    return out.good();
}

}  // namespace modding
