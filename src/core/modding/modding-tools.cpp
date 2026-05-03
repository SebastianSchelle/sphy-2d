#include "modding-tools.hpp"
#include <cctype>
#include <fstream>
#include <magic_enum/magic_enum.hpp>
#include <render-engine.hpp>
#include <save-file-dialog.hpp>
#include <std-inc.hpp>
#include <user-interface.hpp>

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
        "onAddSlot", &ModdingTools::onAddSlot, this);
    moddingToolsConstructor.BindEventCallback(
        "onClearSlots", &ModdingTools::onClearSlots, this);
    moddingToolsConstructor.BindEventCallback(
        "removeSlot", &ModdingTools::onRemoveSlot, this);
    if (auto hullHandle = moddingToolsConstructor.RegisterStruct<GeneralInfo>())
    {
        hullHandle.RegisterMember("name", &GeneralInfo::name);
        hullHandle.RegisterMember("hp", &GeneralInfo::hp);
        hullHandle.RegisterMember("mapIcon", &GeneralInfo::mapIcon);
    }
    moddingToolsConstructor.Bind("hull", &hull);
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

    moddingToolsConstructor.Bind("openFilepath", &openFilepath);
    moddingToolsConstructor.Bind("extendTextures", &extendTextures);
    moddingToolsConstructor.Bind("extendSlots", &extendSlots);

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
    hull = GeneralInfo{};
    textures.clear();
    slots.clear();
    syncModeToRml();
    handle.DirtyVariable("openFilepath");
    handle.DirtyVariable("hull");
    handle.DirtyVariable("textures");
    handle.DirtyVariable("slots");
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
    syncModeToRml();
    handle.DirtyVariable("openFilepath");
    LG_D("Modding tools: new station part");
}

void ModdingTools::onModdingFileSave(Rml::DataModelHandle handle,
                                     Rml::Event& event,
                                     const Rml::VariantList& args)
{
    (void)event;
    (void)args;
    if (activeMode != ModdingToolsMode::Hull)
    {
        return;
    }
    parseEditorNumericFields();
    if (textures.empty())
    {
        LG_W("Modding tools: no textures to save");
        return;
    }
    const string defaultFile =
        sanitizeHullKey(hull.name.empty() ? string("hull") : hull.name) + ".yaml";
    std::string path;
    if (!osh::pick_save_file_path(path, "Save hull as", defaultFile))
    {
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

void ModdingTools::onModdingFileLoad(Rml::DataModelHandle handle,
                                     Rml::Event& event,
                                     const Rml::VariantList& args)
{
    (void)event;
    (void)args;
    std::string path;
    if (!osh::pick_open_file_path(path, "Open hull YAML", openFilepath))
    {
        return;
    }
    if (!loadHullDataFromPath(path))
    {
        LG_W("Modding tools: failed to load hull from {}", path);
        return;
    }
    openFilepath = std::move(path);
    handle.DirtyVariable("openFilepath");
    handle.DirtyVariable("hull");
    handle.DirtyVariable("textures");
    handle.DirtyVariable("slots");
    handle.DirtyVariable("mode");
    LG_D("Modding tools: loaded hull from {}", openFilepath);
}

void ModdingTools::onAddTexture(Rml::DataModelHandle handle,
                                Rml::Event& event,
                                const Rml::VariantList& args)
{
    textures.push_back(TextureInfo{});
    rmlModel_.DirtyVariable("textures");
}

void ModdingTools::onClearTextures(Rml::DataModelHandle handle,
                                   Rml::Event& event,
                                   const Rml::VariantList& args)
{
    textures.clear();
    rmlModel_.DirtyVariable("textures");
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

void ModdingTools::parseEditorNumericFields()
{
    int val;
    tryParseFloat(hull.hp, hpVal);
    floatToString(hpVal, hull.hp, 2);

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
        auto slotType = magic_enum::enum_cast<gobj::ModuleSlotType>(slot.slotType);
        if (slotType.has_value())
        {
            slot.slotTypeVal = slotType.value();
        }
    }
}

void ModdingTools::draw(gfx::RenderEngine& renderer)
{
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

    for (auto& texture : textures)
    {
        auto texHandle = renderer.getTextureHandle(texture.name);
        renderer.drawTexRect(glm::vec2(texture.posXVal, texture.posYVal),
                             glm::vec2(texture.sizeXVal, texture.sizeYVal),
                             texHandle,
                             smath::degToRad(texture.rotVal),
                             0xffffffff,
                             texture.zIndexVal / 100.0f,
                             0);
    }
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
    hull = GeneralInfo{};

    try
    {
        if (hullNode["name"])
        {
            hull.name = hullNode["name"].as<string>();
        }
        if (hull.name.empty())
        {
            hull.name = hullIt->first.as<string>();
        }
        if (hullNode["hullpoints"])
        {
            hpVal = hullNode["hullpoints"].as<float>();
            floatToString(hpVal, hull.hp, 2);
        }
        if (hullNode["map-icon"])
        {
            hull.mapIcon = hullNode["map-icon"].as<string>();
        }

        string texKey;
        if (hullNode["textures"])
        {
            texKey = hullNode["textures"].as<string>();
        }
        const YAML::Node texRoot = root["textures"];
        if (!texKey.empty() && texRoot && texRoot[texKey] && texRoot[texKey]["textures"])
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
            LG_W("Modding tools: hull references textures '{}' but bundle missing in {}",
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
                auto st = magic_enum::enum_cast<gobj::ModuleSlotType>(s.slotType);
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
    }
    catch (const YAML::Exception& e)
    {
        LG_W("Modding tools: error parsing hull data in {}: {}", path, e.what());
        textures.clear();
        slots.clear();
        hull = GeneralInfo{};
        activeMode = ModdingToolsMode::None;
        syncModeToRml();
        return false;
    }

    parseEditorNumericFields();
    return true;
}

bool ModdingTools::saveHullDataToPath(const string& path)
{
    parseEditorNumericFields();

    const string key =
        sanitizeHullKey(hull.name.empty() ? string("hull") : hull.name);
    const string displayName = hull.name.empty() ? key : hull.name;

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
    hullNode["textures"] = key;
    if (!hull.mapIcon.empty())
    {
        hullNode["map-icon"] = hull.mapIcon;
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

    YAML::Node root;
    root["textures"] = YAML::Node(YAML::NodeType::Map);
    root["textures"][key] = texBundle;
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
