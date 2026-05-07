#include "modding-tools.hpp"
#include "texture.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
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

constexpr uint32_t kSelectionTintAbgr = 0xff50ff80u;
constexpr float kTexturePixelToWorld = 0.1f;
constexpr int kHullTextureDefaultZ = 50;
constexpr int kStationPartTextureDefaultZ = 40;
constexpr int kRoofSlotDefaultZ = 49;
constexpr int kNonRoofSlotDefaultZ = 51;
/** Extra size on each axis: fractional + world add (matches ~0.25/zoom at
 * default zoom 2). */
constexpr float kModdingTexDrawOverlapFrac = 0.004f;
constexpr float kModdingTexDrawOverlapAddWorld = 0.125f;
/** Shift-snap: align selected rotation to nearest multiple of this (degrees).
 */
constexpr float kTextureShiftSnapAngleStepDeg = 90.f;
/** When true, texture bounds[2:4] in YAML include bleed; editor keeps logical
 * sizes. */
constexpr const char* kYamlTextureBoundsBleedKey = "texture-bounds-bleed";

/** Quantize `targetDeg` to nearest `stepDeg` multiple, then pick the
 * 360°-equivalent closest to `selectedDeg`. */
float snapAlignedTextureRotationDeg(float targetDeg,
                                    float selectedDeg,
                                    float stepDeg)
{
    float selDeg = fmodf(selectedDeg, 360.f);
    float tarDeg = fmodf(targetDeg, 360.f);
    float angleError = smath::radToDeg(
        smath::angleError(smath::degToRad(tarDeg), smath::degToRad(selDeg)));
    float angleErr = fmodf(angleError, 90.0f);
    if(angleErr > 45.0f)
    {
        angleErr -= 90.0f;
    }
    else if(angleErr < -45.0f)
    {
        angleErr += 90.0f;
    }
    return selectedDeg + angleErr;
}
/** RMB drag: degrees added per world unit of tangential cursor motion, before
 * /zoom. */
constexpr float kModdingRightDragRotate = 5.f;

bool pastDragDeadzone(glm::vec2 pressWorld,
                      glm::vec2 nowWorld,
                      float zoom,
                      float dragThresholdCfg,
                      bool mouseSaysDragActive)
{
    if (mouseSaysDragActive)
    {
        return true;
    }
    const float zz = zoom * zoom;
    if (zz < 1e-12f)
    {
        return true;
    }
    const glm::vec2 w = nowWorld - pressWorld;
    return glm::dot(w, w) >= dragThresholdCfg / zz;
}

int defaultTextureZForMode(ModdingToolsMode mode)
{
    switch (mode)
    {
        case ModdingToolsMode::Hull:
            return kHullTextureDefaultZ;
        case ModdingToolsMode::StationPart:
            return kStationPartTextureDefaultZ;
        default:
            return 0;
    }
}

int defaultSlotZForType(gobj::ModuleSlotType slotType)
{
    switch (slotType)
    {
        case gobj::ModuleSlotType::RoofS_Common:
        case gobj::ModuleSlotType::RoofM_Common:
        case gobj::ModuleSlotType::RoofL_Common:
            return kRoofSlotDefaultZ;
        case gobj::ModuleSlotType::ThrusterMainS_Common:
        case gobj::ModuleSlotType::ThrusterMainM_Common:
        case gobj::ModuleSlotType::ThrusterMainL_Common:
        case gobj::ModuleSlotType::ThrusterManeuverS_Common:
        case gobj::ModuleSlotType::ThrusterManeuverM_Common:
        case gobj::ModuleSlotType::ThrusterManeuverL_Common:
        case gobj::ModuleSlotType::BayS_Common:
        case gobj::ModuleSlotType::BayM_Common:
        case gobj::ModuleSlotType::BayL_Common:
        case gobj::ModuleSlotType::InternalS_Common:
        case gobj::ModuleSlotType::InternalM_Common:
        case gobj::ModuleSlotType::InternalL_Common:
            return kNonRoofSlotDefaultZ;
        default:
            return 0;
    }
}

uint32_t tintIfSelected(uint32_t baseAbgr, bool selected)
{
    if (!selected)
    {
        return baseAbgr;
    }
    const uint8_t a = static_cast<uint8_t>((baseAbgr >> 24) & 0xffu);
    return (static_cast<uint32_t>(a) << 24u)
           | (kSelectionTintAbgr & 0x00ffffffu);
}

bool hitRotatedRect(const glm::vec2& p,
                    const glm::vec2& center,
                    const glm::vec2& size,
                    float rotRad)
{
    const glm::vec2 d = p - center;
    const float c = std::cos(rotRad);
    const float s = std::sin(rotRad);
    const float lx = d.x * c + d.y * s;
    const float ly = -d.x * s + d.y * c;
    return std::fabs(lx) <= size.x * 0.5f && std::fabs(ly) <= size.y * 0.5f;
}

bool hitRotatedEllipse(const glm::vec2& p,
                       const glm::vec2& center,
                       const glm::vec2& size,
                       float rotRad)
{
    const float rx = size.x * 0.5f;
    const float ry = size.y * 0.5f;
    if (rx <= 1e-6f || ry <= 1e-6f)
    {
        return false;
    }
    const glm::vec2 d = p - center;
    const float co = std::cos(rotRad);
    const float si = std::sin(rotRad);
    const float lx = d.x * co + d.y * si;
    const float ly = -d.x * si + d.y * co;
    const float nx = lx / rx;
    const float ny = ly / ry;
    return nx * nx + ny * ny <= 1.0f;
}

bool hitDisc(const glm::vec2& p, const glm::vec2& center, float radius)
{
    const glm::vec2 d = p - center;
    return glm::dot(d, d) <= radius * radius;
}

bool hitRoofLikeSlot(const glm::vec2& p, const SlotInfo& slot)
{
    const float rot = smath::degToRad(slot.rotVal);
    const float s = gobj::SlotSize[static_cast<size_t>(slot.slotTypeVal)];
    const glm::vec2 center(slot.posXVal, slot.posYVal);
    if (hitRotatedEllipse(p, center, glm::vec2(s, s), rot))
    {
        return true;
    }
    const glm::vec2 offs = smath::rotateVec2(glm::vec2(0.0f, s), rot);
    return hitRotatedRect(p, center + offs, glm::vec2(s * 0.3f, s), rot);
}

bool hitThrusterMainSlot(const glm::vec2& p, const SlotInfo& slot)
{
    const float s = gobj::SlotSize[static_cast<size_t>(slot.slotTypeVal)];
    const float rot = smath::degToRad(slot.rotVal);
    const glm::vec2 center(slot.posXVal, slot.posYVal);
    if (hitRotatedRect(p, center, glm::vec2(s, s * 2.0f), rot))
    {
        return true;
    }
    const glm::vec2 offs = smath::rotateVec2(glm::vec2(0.0f, s * 2.0f), rot);
    return hitRotatedRect(p, center - offs, glm::vec2(s * 0.6f, s * 2.0f), rot);
}

bool hitThrusterManeuverSlot(const glm::vec2& p, const SlotInfo& slot)
{
    const float s = gobj::SlotSize[static_cast<size_t>(slot.slotTypeVal)];
    const float rot = smath::degToRad(slot.rotVal);
    const glm::vec2 center(slot.posXVal, slot.posYVal);
    if (hitRotatedRect(p, center, glm::vec2(s, s * 0.5f), rot))
    {
        return true;
    }
    const glm::vec2 offs = smath::rotateVec2(glm::vec2(0.0f, s * 0.5f), rot);
    return hitRotatedRect(p, center - offs, glm::vec2(s * 0.6f, s * 0.5f), rot);
}

bool hitInternalSlot(const glm::vec2& p, const SlotInfo& slot)
{
    const float s = gobj::SlotSize[static_cast<size_t>(slot.slotTypeVal)];
    const float rot = smath::degToRad(slot.rotVal);
    return hitRotatedRect(
        p, glm::vec2(slot.posXVal, slot.posYVal), glm::vec2(s, s), rot);
}

bool hitBaySlot(const glm::vec2& p, const SlotInfo& slot)
{
    const float s = gobj::SlotSize[static_cast<size_t>(slot.slotTypeVal)];
    const float rot = smath::degToRad(slot.rotVal);
    return hitRotatedRect(
        p, glm::vec2(slot.posXVal, slot.posYVal), glm::vec2(s, s * 0.2f), rot);
}

bool hitSlot(const glm::vec2& p, const SlotInfo& slot)
{
    switch (slot.slotTypeVal)
    {
        case gobj::ModuleSlotType::RoofS_Common:
        case gobj::ModuleSlotType::RoofM_Common:
        case gobj::ModuleSlotType::RoofL_Common:
            return hitRoofLikeSlot(p, slot);
        case gobj::ModuleSlotType::ThrusterMainS_Common:
        case gobj::ModuleSlotType::ThrusterMainM_Common:
        case gobj::ModuleSlotType::ThrusterMainL_Common:
            return hitThrusterMainSlot(p, slot);
        case gobj::ModuleSlotType::ThrusterManeuverS_Common:
        case gobj::ModuleSlotType::ThrusterManeuverM_Common:
        case gobj::ModuleSlotType::ThrusterManeuverL_Common:
            return hitThrusterManeuverSlot(p, slot);
        case gobj::ModuleSlotType::InternalS_Common:
        case gobj::ModuleSlotType::InternalM_Common:
        case gobj::ModuleSlotType::InternalL_Common:
            return hitInternalSlot(p, slot);
        case gobj::ModuleSlotType::BayS_Common:
        case gobj::ModuleSlotType::BayM_Common:
        case gobj::ModuleSlotType::BayL_Common:
            return hitBaySlot(p, slot);
        default:
            return false;
    }
}

bool hitTexture(const glm::vec2& p, const TextureInfo& tex)
{
    return hitRotatedRect(p,
                          glm::vec2(tex.posXVal, tex.posYVal),
                          glm::vec2(tex.sizeXVal, tex.sizeYVal),
                          smath::degToRad(tex.rotVal));
}

enum class TextureRectEdge
{
    Right,
    Left,
    Top,
    Bottom,
};

/** Closest edge of anchor texture (in anchor local space) to worldPos; must be
 * inside rect. */
TextureRectEdge closestTextureEdgeLocal(const glm::vec2& worldPos,
                                        const TextureInfo& anchor)
{
    const float theta = smath::degToRad(anchor.rotVal);
    const float c = std::cos(theta);
    const float s = std::sin(theta);
    const glm::vec2 d(worldPos.x - anchor.posXVal, worldPos.y - anchor.posYVal);
    const float lx = d.x * c + d.y * s;
    const float ly = -d.x * s + d.y * c;
    const float hw = anchor.sizeXVal * 0.5f;
    const float hh = anchor.sizeYVal * 0.5f;
    const float distLeft = lx + hw;
    const float distRight = hw - lx;
    const float distBottom = ly + hh;
    const float distTop = hh - ly;
    struct
    {
        float dist;
        TextureRectEdge edge;
    } cands[] = {{distRight, TextureRectEdge::Right},
                 {distLeft, TextureRectEdge::Left},
                 {distTop, TextureRectEdge::Top},
                 {distBottom, TextureRectEdge::Bottom}};
    size_t best = 0;
    for (size_t i = 1; i < 4; ++i)
    {
        if (cands[i].dist < cands[best].dist)
        {
            best = i;
        }
    }
    return cands[best].edge;
}

void shiftAlignSelectedTextureToAnchor(TextureInfo& selected,
                                       const TextureInfo& anchor,
                                       const glm::vec2& worldPos)
{
    const float prevRotDeg = selected.rotVal;
    const float snappedRotDeg = snapAlignedTextureRotationDeg(
        anchor.rotVal, prevRotDeg, kTextureShiftSnapAngleStepDeg);
    const TextureRectEdge edge = closestTextureEdgeLocal(worldPos, anchor);
    const float theta = smath::degToRad(anchor.rotVal);
    const float c = std::cos(theta);
    const float s = std::sin(theta);
    const float w1 = selected.sizeXVal;
    const float h1 = selected.sizeYVal;
    const float w2 = anchor.sizeXVal;
    const float h2 = anchor.sizeYVal;
    /** Relative rotation (snapped selected − anchor): projects selected W/H onto
     *  anchor local axes for edge–edge separation (OBB half-extent along axis). */
    const float ds = smath::degToRad(snappedRotDeg - anchor.rotVal);
    const float cRel = std::fabs(std::cos(ds));
    const float sRel = std::fabs(std::sin(ds));
    const float halfProjAlongAnchorX = 0.5f * (w1 * cRel + h1 * sRel);
    const float halfProjAlongAnchorY = 0.5f * (w1 * sRel + h1 * cRel);
    const float halfW2 = 0.5f * w2;
    const float halfH2 = 0.5f * h2;
    float olx = 0.0f;
    float oly = 0.0f;
    switch (edge)
    {
        case TextureRectEdge::Right:
            olx = halfW2 + halfProjAlongAnchorX;
            break;
        case TextureRectEdge::Left:
            olx = -(halfW2 + halfProjAlongAnchorX);
            break;
        case TextureRectEdge::Top:
            oly = halfH2 + halfProjAlongAnchorY;
            break;
        case TextureRectEdge::Bottom:
            oly = -(halfH2 + halfProjAlongAnchorY);
            break;
    }
    const float dx = c * olx - s * oly;
    const float dy = s * olx + c * oly;
    selected.posXVal = anchor.posXVal + dx;
    selected.posYVal = anchor.posYVal + dy;
    selected.rotVal = snappedRotDeg;
    floatToString(selected.posXVal, selected.posX, 2);
    floatToString(selected.posYVal, selected.posY, 2);
    floatToString(selected.rotVal, selected.rot, 2);
}

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
    std::transform(out.begin(),
                   out.end(),
                   out.begin(),
                   [](unsigned char c)
                   { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool textureNameUsesBoundsBleed(const string& name)
{
    return toLowerCopy(name) != "station-connector";
}

glm::vec2 moddingTextureLogicalSizeToAsset(float sx, float sy, bool applyBleed)
{
    if (!applyBleed || sx <= 0.0f || sy <= 0.0f)
    {
        return {sx, sy};
    }
    return {sx * (1.0f + kModdingTexDrawOverlapFrac)
                + kModdingTexDrawOverlapAddWorld,
            sy * (1.0f + kModdingTexDrawOverlapFrac)
                + kModdingTexDrawOverlapAddWorld};
}

glm::vec2 moddingTextureAssetSizeToLogical(float sx, float sy, bool applyBleed)
{
    if (!applyBleed || sx <= 0.0f || sy <= 0.0f)
    {
        return {sx, sy};
    }
    const float denom = 1.0f + kModdingTexDrawOverlapFrac;
    const float lx = (sx - kModdingTexDrawOverlapAddWorld) / denom;
    const float ly = (sy - kModdingTexDrawOverlapAddWorld) / denom;
    return {std::max(lx, 1e-6f), std::max(ly, 1e-6f)};
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

void ModdingTools::syncListSelectionToRml()
{
    if (!rmlModel_)
    {
        return;
    }
    if (selectedObjectType == SelectableObjectType::None
        || selectedObjectIndex < 0)
    {
        selectedListKind = "None";
        selectedListIndex = -1;
        activeTextureIndex = -1;
    }
    else
    {
        selectedListKind =
            std::string(magic_enum::enum_name(selectedObjectType));
        selectedListIndex = selectedObjectIndex;
        activeTextureIndex =
            selectedObjectType == SelectableObjectType::Texture
                ? selectedObjectIndex
                : -1;
        switch (selectedObjectType)
        {
            case SelectableObjectType::Texture:
                extendTextures = true;
                break;
            case SelectableObjectType::Slot:
                extendSlots = true;
                break;
            case SelectableObjectType::ColliderVertex:
                extendCollider = true;
                break;
            case SelectableObjectType::Connector:
                extendConnectors = true;
                break;
            default:
                break;
        }
    }
    if (selectedObjectType != SelectableObjectType::Texture
        || selectedObjectIndex < 0)
    {
        textureRowNameFocusIndex = -1;
    }
    else if (textureRowNameFocusIndex >= 0
             && textureRowNameFocusIndex != selectedObjectIndex)
    {
        textureRowNameFocusIndex = -1;
    }
    rmlModel_.DirtyVariable("selectedListKind");
    rmlModel_.DirtyVariable("selectedListIndex");
    rmlModel_.DirtyVariable("activeTextureIndex");
    rmlModel_.DirtyVariable("textureRowNameFocusIndex");
    rmlModel_.DirtyVariable("extendTextures");
    rmlModel_.DirtyVariable("extendSlots");
    rmlModel_.DirtyVariable("extendCollider");
    rmlModel_.DirtyVariable("extendConnectors");
}

void ModdingTools::fixSelectionAfterErase(SelectableObjectType listKind,
                                            int erasedIndex)
{
    if (selectedObjectType != listKind)
    {
        return;
    }
    if (selectedObjectIndex == erasedIndex)
    {
        selectedObjectType = SelectableObjectType::None;
        selectedObjectIndex = -1;
    }
    else if (selectedObjectIndex > erasedIndex)
    {
        selectedObjectIndex--;
    }
}

void ModdingTools::onModdingSelectListRow(Rml::DataModelHandle handle,
                                          Rml::Event& event,
                                          const Rml::VariantList& args)
{
    (void)event;
    if (args.size() != 2)
    {
        return;
    }
    const string kindStr = args[0].Get<string>("");
    const int idx = args[1].Get<int>(-1);
    const auto kind = magic_enum::enum_cast<SelectableObjectType>(kindStr);
    if (!kind || *kind == SelectableObjectType::None)
    {
        selectedObjectType = SelectableObjectType::None;
        selectedObjectIndex = -1;
        syncListSelectionToRml();
        return;
    }
    switch (*kind)
    {
        case SelectableObjectType::Texture:
            if (activeMode != ModdingToolsMode::Hull
                && activeMode != ModdingToolsMode::StationPart)
            {
                return;
            }
            if (idx < 0 || idx >= static_cast<int>(textures.size()))
            {
                return;
            }
            break;
        case SelectableObjectType::Slot:
            if (activeMode != ModdingToolsMode::Hull)
            {
                return;
            }
            if (idx < 0 || idx >= static_cast<int>(slots.size()))
            {
                return;
            }
            break;
        case SelectableObjectType::ColliderVertex:
            if (activeMode != ModdingToolsMode::Hull
                && activeMode != ModdingToolsMode::StationPart)
            {
                return;
            }
            if (idx < 0 || idx >= static_cast<int>(collider.size()))
            {
                return;
            }
            break;
        case SelectableObjectType::Connector:
            if (activeMode != ModdingToolsMode::StationPart)
            {
                return;
            }
            if (idx < 0 || idx >= static_cast<int>(connectors.size()))
            {
                return;
            }
            break;
        default:
            return;
    }
    selectedObjectType = *kind;
    selectedObjectIndex = idx;
    syncListSelectionToRml();
}

void ModdingTools::onSingleClick(const glm::vec2& worldPos,
                                 float worldZoom,
                                 gfx::RenderEngine* renderer,
                                 bool shiftAlignTextures)
{
    suppressDragAfterClick = false;

    if (activeMode != ModdingToolsMode::Hull
        && activeMode != ModdingToolsMode::StationPart)
    {
        return;
    }

    const float colliderPickR = 3.0f / worldZoom;
    const float connectorPickR = 4.0f / worldZoom;

    auto clearSelection = [this]()
    {
        selectedObjectType = SelectableObjectType::None;
        selectedObjectIndex = -1;
        syncListSelectionToRml();
    };

    /** Lower `z` = foreground. When `z` matches, higher `layerTie` wins (later
     * in draw order). Then lower `index` for stability. */
    struct PickCandidate
    {
        SelectableObjectType type = SelectableObjectType::None;
        int index = -1;
        int z = 0;
        int layerTie = 0;
    };
    auto betterForeground = [](const PickCandidate& a,
                               const PickCandidate& b) -> bool
    {
        if (a.z != b.z)
        {
            return a.z < b.z;
        }
        if (a.layerTie != b.layerTie)
        {
            return a.layerTie > b.layerTie;
        }
        return a.index < b.index;
    };
    PickCandidate bestPick{};
    bestPick.index = -1;
    auto consider = [&](SelectableObjectType type,
                        int index,
                        int z,
                        int layerTie)
    {
        PickCandidate c{type, index, z, layerTie};
        if (bestPick.index < 0 || betterForeground(c, bestPick))
        {
            bestPick = c;
        }
    };

    /** Implicit z for collider dots / connectors (no zIndex field); competes
     * with texture/slot zIndexVal==0. */
    constexpr int kPickImplicitZ = 0;
    constexpr int kPickHullLayerTex = 0;
    constexpr int kPickHullLayerSlot = 1;
    constexpr int kPickHullLayerCollider = 2;
    constexpr int kPickPartLayerTex = 0;
    constexpr int kPickPartLayerCollider = 1;
    constexpr int kPickPartLayerConnector = 2;

    if (activeMode == ModdingToolsMode::Hull)
    {
        for (int i = 0; i < static_cast<int>(collider.size()); ++i)
        {
            const glm::vec2 v(collider[static_cast<size_t>(i)].xVal,
                              collider[static_cast<size_t>(i)].yVal);
            if (hitDisc(worldPos, v, colliderPickR))
            {
                consider(SelectableObjectType::ColliderVertex,
                         i,
                         kPickImplicitZ,
                         kPickHullLayerCollider);
            }
        }
        for (int i = 0; i < static_cast<int>(slots.size()); ++i)
        {
            if (hitSlot(worldPos, slots[static_cast<size_t>(i)]))
            {
                consider(SelectableObjectType::Slot,
                         i,
                         slots[static_cast<size_t>(i)].zIndexVal,
                         kPickHullLayerSlot);
            }
        }
    }
    else if (activeMode == ModdingToolsMode::StationPart)
    {
        for (int i = 0; i < static_cast<int>(connectors.size()); ++i)
        {
            const glm::vec2 c(connectors[static_cast<size_t>(i)].posXVal,
                              connectors[static_cast<size_t>(i)].posYVal);
            if (hitDisc(worldPos, c, connectorPickR))
            {
                consider(SelectableObjectType::Connector,
                         i,
                         kPickImplicitZ,
                         kPickPartLayerConnector);
            }
        }
        for (int i = 0; i < static_cast<int>(collider.size()); ++i)
        {
            const glm::vec2 v(collider[static_cast<size_t>(i)].xVal,
                              collider[static_cast<size_t>(i)].yVal);
            if (hitDisc(worldPos, v, colliderPickR))
            {
                consider(SelectableObjectType::ColliderVertex,
                         i,
                         kPickImplicitZ,
                         kPickPartLayerCollider);
            }
        }
    }

    for (int i = 0; i < static_cast<int>(textures.size()); ++i)
    {
        if (hitTexture(worldPos, textures[static_cast<size_t>(i)]))
        {
            const int layer = activeMode == ModdingToolsMode::Hull
                                  ? kPickHullLayerTex
                                  : kPickPartLayerTex;
            consider(SelectableObjectType::Texture,
                     i,
                     textures[static_cast<size_t>(i)].zIndexVal,
                     layer);
        }
    }

    auto applyPick = [this](const PickCandidate& p)
    {
        selectedObjectType = p.type;
        selectedObjectIndex = p.index;
        syncListSelectionToRml();
    };

    if (shiftAlignTextures
        && selectedObjectType == SelectableObjectType::Texture
        && selectedObjectIndex >= 0
        && selectedObjectIndex < static_cast<int>(textures.size()))
    {
        int anchorIdx = -1;
        int bestZ = 0;
        for (int i = 0; i < static_cast<int>(textures.size()); ++i)
        {
            if (i == selectedObjectIndex)
            {
                continue;
            }
            if (!hitTexture(worldPos, textures[static_cast<size_t>(i)]))
            {
                continue;
            }
            const int zi = textures[static_cast<size_t>(i)].zIndexVal;
            if (anchorIdx < 0 || zi < bestZ
                || (zi == bestZ && i < anchorIdx))
            {
                anchorIdx = i;
                bestZ = zi;
            }
        }
        if (anchorIdx >= 0)
        {
            shiftAlignSelectedTextureToAnchor(
                textures[static_cast<size_t>(selectedObjectIndex)],
                textures[static_cast<size_t>(anchorIdx)],
                worldPos);
            suppressDragAfterClick = true;
            if (rmlModel_)
            {
                rmlModel_.DirtyVariable("textures");
            }
            return;
        }
    }

    if (bestPick.index >= 0)
    {
        applyPick(bestPick);
        return;
    }

    clearSelection();
}

void ModdingTools::onLeftMouseDown(const glm::vec2& worldPos,
                                   float worldZoom,
                                   float dragThresholdWorld,
                                   gfx::RenderEngine* renderer,
                                   bool shiftAlignTextures)
{
    (void)worldZoom;
    lmbDragPressWorld = worldPos;
    lmbDragThresholdCfg = dragThresholdWorld;
    lmbPastDragDeadzone = false;
    onSingleClick(worldPos, worldZoom, renderer, shiftAlignTextures);
    dragSelectedObject = selectedObjectType != SelectableObjectType::None
                         && !suppressDragAfterClick;
}

void ModdingTools::onLeftMouseDrag(const glm::vec2& worldPos,
                                   float worldZoom,
                                   bool mouseDragActiveLmb)
{
    if (!dragSelectedObject || selectedObjectIndex < 0)
    {
        return;
    }
    if (!lmbPastDragDeadzone)
    {
        if (!pastDragDeadzone(lmbDragPressWorld,
                              worldPos,
                              worldZoom,
                              lmbDragThresholdCfg,
                              mouseDragActiveLmb))
        {
            return;
        }
        lmbPastDragDeadzone = true;
    }

    switch (selectedObjectType)
    {
        case SelectableObjectType::Texture:
            if (selectedObjectIndex >= static_cast<int>(textures.size()))
            {
                return;
            }
            textures[static_cast<size_t>(selectedObjectIndex)].posXVal =
                worldPos.x;
            textures[static_cast<size_t>(selectedObjectIndex)].posYVal =
                worldPos.y;
            floatToString(
                textures[static_cast<size_t>(selectedObjectIndex)].posXVal,
                textures[static_cast<size_t>(selectedObjectIndex)].posX,
                2);
            floatToString(
                textures[static_cast<size_t>(selectedObjectIndex)].posYVal,
                textures[static_cast<size_t>(selectedObjectIndex)].posY,
                2);
            rmlModel_.DirtyVariable("textures");
            break;
        case SelectableObjectType::Slot:
            if (selectedObjectIndex >= static_cast<int>(slots.size()))
            {
                return;
            }
            slots[static_cast<size_t>(selectedObjectIndex)].posXVal =
                worldPos.x;
            slots[static_cast<size_t>(selectedObjectIndex)].posYVal =
                worldPos.y;
            floatToString(
                slots[static_cast<size_t>(selectedObjectIndex)].posXVal,
                slots[static_cast<size_t>(selectedObjectIndex)].posX,
                2);
            floatToString(
                slots[static_cast<size_t>(selectedObjectIndex)].posYVal,
                slots[static_cast<size_t>(selectedObjectIndex)].posY,
                2);
            rmlModel_.DirtyVariable("slots");
            break;
        case SelectableObjectType::ColliderVertex:
            if (selectedObjectIndex >= static_cast<int>(collider.size()))
            {
                return;
            }
            collider[static_cast<size_t>(selectedObjectIndex)].xVal =
                worldPos.x;
            collider[static_cast<size_t>(selectedObjectIndex)].yVal =
                worldPos.y;
            floatToString(
                collider[static_cast<size_t>(selectedObjectIndex)].xVal,
                collider[static_cast<size_t>(selectedObjectIndex)].x,
                2);
            floatToString(
                collider[static_cast<size_t>(selectedObjectIndex)].yVal,
                collider[static_cast<size_t>(selectedObjectIndex)].y,
                2);
            rmlModel_.DirtyVariable("collider");
            break;
        case SelectableObjectType::Connector:
            if (selectedObjectIndex >= static_cast<int>(connectors.size()))
            {
                return;
            }
            connectors[static_cast<size_t>(selectedObjectIndex)].posXVal =
                worldPos.x;
            connectors[static_cast<size_t>(selectedObjectIndex)].posYVal =
                worldPos.y;
            floatToString(
                connectors[static_cast<size_t>(selectedObjectIndex)].posXVal,
                connectors[static_cast<size_t>(selectedObjectIndex)].posX,
                2);
            floatToString(
                connectors[static_cast<size_t>(selectedObjectIndex)].posYVal,
                connectors[static_cast<size_t>(selectedObjectIndex)].posY,
                2);
            rmlModel_.DirtyVariable("connectors");
            if (activeMode == ModdingToolsMode::StationPart)
            {
                syncStationPartConnectorTextures();
                rmlModel_.DirtyVariable("textures");
            }
            break;
        case SelectableObjectType::None:
        default:
            break;
    }
}

void ModdingTools::onLeftMouseUp()
{
    dragSelectedObject = false;
    lmbPastDragDeadzone = false;
}

void ModdingTools::onRightMouseDown(const glm::vec2& worldPos,
                                    float dragThresholdWorld)
{
    rmbDragPressWorld = worldPos;
    rmbDragThresholdCfg = dragThresholdWorld;
    rmbPastDragDeadzone = false;
    rmbRotatePrevWorld = worldPos;
    rmbRotateGesture = false;
    if (activeMode != ModdingToolsMode::Hull
        && activeMode != ModdingToolsMode::StationPart)
    {
        return;
    }
    switch (selectedObjectType)
    {
        case SelectableObjectType::Texture:
            if (selectedObjectIndex >= 0
                && selectedObjectIndex < static_cast<int>(textures.size()))
            {
                rmbRotateGesture = true;
            }
            break;
        case SelectableObjectType::Slot:
            if (selectedObjectIndex >= 0
                && selectedObjectIndex < static_cast<int>(slots.size()))
            {
                rmbRotateGesture = true;
            }
            break;
        case SelectableObjectType::Connector:
            if (activeMode == ModdingToolsMode::StationPart
                && selectedObjectIndex >= 0
                && selectedObjectIndex < static_cast<int>(connectors.size()))
            {
                rmbRotateGesture = true;
            }
            break;
        default:
            break;
    }
}

void ModdingTools::onRightMouseDrag(const glm::vec2& worldPos,
                                    float worldZoom,
                                    bool mouseDragActiveRmb)
{
    if (!rmbRotateGesture || worldZoom < 1e-6f)
    {
        return;
    }
    if (!rmbPastDragDeadzone)
    {
        if (!pastDragDeadzone(rmbDragPressWorld,
                              worldPos,
                              worldZoom,
                              rmbDragThresholdCfg,
                              mouseDragActiveRmb))
        {
            return;
        }
        rmbPastDragDeadzone = true;
    }

    glm::vec2 pivot{};
    float* rotDeg = nullptr;
    switch (selectedObjectType)
    {
        case SelectableObjectType::Texture:
        {
            if (selectedObjectIndex < 0
                || selectedObjectIndex >= static_cast<int>(textures.size()))
            {
                return;
            }
            TextureInfo& t = textures[static_cast<size_t>(selectedObjectIndex)];
            pivot = {t.posXVal, t.posYVal};
            rotDeg = &t.rotVal;
            break;
        }
        case SelectableObjectType::Slot:
        {
            if (selectedObjectIndex < 0
                || selectedObjectIndex >= static_cast<int>(slots.size()))
            {
                return;
            }
            SlotInfo& s = slots[static_cast<size_t>(selectedObjectIndex)];
            pivot = {s.posXVal, s.posYVal};
            rotDeg = &s.rotVal;
            break;
        }
        case SelectableObjectType::Connector:
        {
            if (selectedObjectIndex < 0
                || selectedObjectIndex >= static_cast<int>(connectors.size()))
            {
                return;
            }
            ConnectorInfo& c =
                connectors[static_cast<size_t>(selectedObjectIndex)];
            pivot = {c.posXVal, c.posYVal};
            rotDeg = &c.rotDegVal;
            break;
        }
        default:
            return;
    }

    const glm::vec2 d = worldPos - rmbRotatePrevWorld;
    if (glm::dot(d, d) < 1e-12f)
    {
        return;
    }
    const glm::vec2 toM = worldPos - pivot;
    if (glm::dot(toM, toM) < 1e-8f)
    {
        rmbRotatePrevWorld = worldPos;
        return;
    }
    const glm::vec2 radial = glm::normalize(toM);
    const float tangential = d.x * (-radial.y) + d.y * radial.x;
    *rotDeg += kModdingRightDragRotate * tangential / worldZoom;
    rmbRotatePrevWorld = worldPos;

    switch (selectedObjectType)
    {
        case SelectableObjectType::Texture:
        {
            TextureInfo& t = textures[static_cast<size_t>(selectedObjectIndex)];
            floatToString(t.rotVal, t.rot, 2);
            rmlModel_.DirtyVariable("textures");
            break;
        }
        case SelectableObjectType::Slot:
        {
            SlotInfo& s = slots[static_cast<size_t>(selectedObjectIndex)];
            floatToString(s.rotVal, s.rot, 2);
            rmlModel_.DirtyVariable("slots");
            break;
        }
        case SelectableObjectType::Connector:
        {
            ConnectorInfo& c =
                connectors[static_cast<size_t>(selectedObjectIndex)];
            floatToString(c.rotDegVal, c.rot, 2);
            rmlModel_.DirtyVariable("connectors");
            if (activeMode == ModdingToolsMode::StationPart)
            {
                syncStationPartConnectorTextures();
                rmlModel_.DirtyVariable("textures");
            }
            break;
        }
        default:
            break;
    }
}

void ModdingTools::onRightMouseUp()
{
    rmbRotateGesture = false;
    rmbPastDragDeadzone = false;
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
        "onGlobalNewTexturePickerFocus",
        &ModdingTools::onGlobalNewTexturePickerFocus,
        this);
    moddingToolsConstructor.BindEventCallback(
        "onTextureRowNonNameFocus", &ModdingTools::onTextureRowNonNameFocus, this);
    moddingToolsConstructor.BindEventCallback(
        "onPickTextureName", &ModdingTools::onPickTextureName, this);
    moddingToolsConstructor.BindEventCallback(
        "onPickNewTextureNameFromPicker",
        &ModdingTools::onPickNewTextureNameFromPicker,
        this);
    moddingToolsConstructor.BindEventCallback(
        "onModdingSelectListRow", &ModdingTools::onModdingSelectListRow, this);
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
    moddingToolsConstructor.RegisterArray<std::vector<string>>();
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
        textureHandle.RegisterMember("nameSuggestions",
                                     &TextureInfo::nameSuggestions);
    }
    moddingToolsConstructor.RegisterArray<std::vector<TextureInfo>>();
    moddingToolsConstructor.Bind("textures", &textures);
    moddingToolsConstructor.Bind("renderTextureNames", &renderTextureNames);
    moddingToolsConstructor.Bind("activeTextureIndex", &activeTextureIndex);
    moddingToolsConstructor.Bind("selectedListKind", &selectedListKind);
    moddingToolsConstructor.Bind("selectedListIndex", &selectedListIndex);
    moddingToolsConstructor.Bind("textureRowNameFocusIndex",
                                 &textureRowNameFocusIndex);
    moddingToolsConstructor.Bind("newTexturePickerName", &newTexturePickerName);
    moddingToolsConstructor.Bind("filteredNewTexturePickerNames",
                                 &filteredNewTexturePickerNames);
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
    textureSizeAppliedForName.clear();
    resetNewTexturePickerState();
    slots.clear();
    collider.clear();
    connectors.clear();
    genInfo.colliderRestitutionVal = 0.1f;
    selectedObjectType = SelectableObjectType::None;
    selectedObjectIndex = -1;
    textureRowNameFocusIndex = -1;
    syncModeToRml();
    handle.DirtyVariable("openFilepath");
    handle.DirtyVariable("hull");
    handle.DirtyVariable("stationPart");
    handle.DirtyVariable("textures");
    handle.DirtyVariable("slots");
    handle.DirtyVariable("collider");
    handle.DirtyVariable("connectors");
    syncListSelectionToRml();
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
    textureSizeAppliedForName.clear();
    resetNewTexturePickerState();
    slots.clear();
    collider.clear();
    connectors.clear();
    genInfo.colliderRestitutionVal = 0.1f;
    hpVal = 1000.0f;
    floatToString(hpVal, genInfo.hp, 2);
    selectedObjectType = SelectableObjectType::None;
    selectedObjectIndex = -1;
    textureRowNameFocusIndex = -1;
    syncModeToRml();
    handle.DirtyVariable("openFilepath");
    handle.DirtyVariable("hull");
    handle.DirtyVariable("stationPart");
    handle.DirtyVariable("textures");
    handle.DirtyVariable("collider");
    handle.DirtyVariable("connectors");
    syncListSelectionToRml();
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
    textureSizeAppliedForName.resize(textures.size());
    for (size_t i = 0; i < textures.size(); ++i)
    {
        textureSizeAppliedForName[i] = textures[i].name;
    }
    resetNewTexturePickerState();
    handle.DirtyVariable("newTexturePickerName");
    handle.DirtyVariable("filteredNewTexturePickerNames");
    selectedObjectType = SelectableObjectType::None;
    selectedObjectIndex = -1;
    textureRowNameFocusIndex = -1;
    syncListSelectionToRml();
}

void ModdingTools::onAddTexture(Rml::DataModelHandle handle,
                                Rml::Event& event,
                                const Rml::VariantList& args)
{
    (void)event;
    (void)args;
    textures.push_back(makeNewTextureEntry());
    textureSizeAppliedForName.push_back(string());
    selectedObjectType = SelectableObjectType::Texture;
    selectedObjectIndex = static_cast<int>(textures.size()) - 1;
    rmlModel_.DirtyVariable("textures");
    syncListSelectionToRml();
}

void ModdingTools::onClearTextures(Rml::DataModelHandle handle,
                                   Rml::Event& event,
                                   const Rml::VariantList& args)
{
    textures.clear();
    textureSizeAppliedForName.clear();
    textureRowNameFocusIndex = -1;
    resetNewTexturePickerState();
    if (selectedObjectType == SelectableObjectType::Texture)
    {
        selectedObjectType = SelectableObjectType::None;
        selectedObjectIndex = -1;
    }
    rmlModel_.DirtyVariable("textures");
    syncListSelectionToRml();
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
        if (i < static_cast<int>(textureSizeAppliedForName.size()))
        {
            textureSizeAppliedForName.erase(textureSizeAppliedForName.begin()
                                            + static_cast<size_t>(i));
        }
        fixSelectionAfterErase(SelectableObjectType::Texture, i);
        if (textureRowNameFocusIndex == i)
        {
            textureRowNameFocusIndex = -1;
        }
        else if (textureRowNameFocusIndex > i)
        {
            textureRowNameFocusIndex--;
        }
        handle.DirtyVariable("textures");
        syncListSelectionToRml();
    }
}

void ModdingTools::onTextureNameFocus(Rml::DataModelHandle handle,
                                      Rml::Event& event,
                                      const Rml::VariantList& args)
{
    (void)event;
    (void)handle;
    if (args.size() != 1)
    {
        return;
    }
    const int i = args[0].Get<int>(-1);
    if (i < 0 || i >= static_cast<int>(textures.size()))
    {
        return;
    }
    textureRowNameFocusIndex = i;
    selectedObjectType = SelectableObjectType::Texture;
    selectedObjectIndex = i;
    syncListSelectionToRml();
}

void ModdingTools::onGlobalNewTexturePickerFocus(Rml::DataModelHandle handle,
                                                 Rml::Event& event,
                                                 const Rml::VariantList& args)
{
    (void)event;
    (void)handle;
    (void)args;
    textureRowNameFocusIndex = -1;
    if (rmlModel_)
    {
        rmlModel_.DirtyVariable("textureRowNameFocusIndex");
    }
}

void ModdingTools::onTextureRowNonNameFocus(Rml::DataModelHandle handle,
                                            Rml::Event& event,
                                            const Rml::VariantList& args)
{
    (void)handle;
    (void)event;
    (void)args;
    textureRowNameFocusIndex = -1;
    if (rmlModel_)
    {
        rmlModel_.DirtyVariable("textureRowNameFocusIndex");
    }
}

void ModdingTools::onPickNewTextureNameFromPicker(Rml::DataModelHandle handle,
                                                  Rml::Event& event,
                                                  const Rml::VariantList& args)
{
    (void)event;
    if (args.size() != 1)
    {
        return;
    }
    if (activeMode != ModdingToolsMode::Hull
        && activeMode != ModdingToolsMode::StationPart)
    {
        return;
    }
    const string picked = args[0].Get<string>("");
    if (picked.empty())
    {
        return;
    }
    TextureInfo texture = makeNewTextureEntry();
    texture.name = picked;
    textures.push_back(std::move(texture));
    textureSizeAppliedForName.push_back(string());
    selectedObjectType = SelectableObjectType::Texture;
    selectedObjectIndex = static_cast<int>(textures.size()) - 1;
    handle.DirtyVariable("textures");
    syncListSelectionToRml();
}

void ModdingTools::onPickTextureName(Rml::DataModelHandle handle,
                                     Rml::Event& event,
                                     const Rml::VariantList& args)
{
    (void)event;
    if (args.size() != 2)
    {
        return;
    }
    const int i = args[0].Get<int>(-1);
    const string picked = args[1].Get<string>("");
    if (i < 0 || i >= static_cast<int>(textures.size()) || picked.empty())
    {
        return;
    }
    textures[static_cast<size_t>(i)].name = picked;
    handle.DirtyVariable("textures");
}

void ModdingTools::onAddSlot(Rml::DataModelHandle handle,
                             Rml::Event& event,
                             const Rml::VariantList& args)
{
    (void)event;
    (void)args;
    SlotInfo slot;
    slot.zIndexVal = defaultSlotZForType(slot.slotTypeVal);
    intToString(slot.zIndexVal, slot.zIndex);
    slots.push_back(std::move(slot));
    rmlModel_.DirtyVariable("slots");
}

void ModdingTools::onClearSlots(Rml::DataModelHandle handle,
                                Rml::Event& event,
                                const Rml::VariantList& args)
{
    slots.clear();
    if (selectedObjectType == SelectableObjectType::Slot)
    {
        selectedObjectType = SelectableObjectType::None;
        selectedObjectIndex = -1;
    }
    handle.DirtyVariable("slots");
    syncListSelectionToRml();
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
        fixSelectionAfterErase(SelectableObjectType::Slot, i);
        handle.DirtyVariable("slots");
        syncListSelectionToRml();
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
    if (selectedObjectType == SelectableObjectType::ColliderVertex)
    {
        selectedObjectType = SelectableObjectType::None;
        selectedObjectIndex = -1;
    }
    rmlModel_.DirtyVariable("collider");
    syncListSelectionToRml();
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
        fixSelectionAfterErase(SelectableObjectType::ColliderVertex, i);
        handle.DirtyVariable("collider");
        syncListSelectionToRml();
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
    if (selectedObjectType == SelectableObjectType::Connector)
    {
        selectedObjectType = SelectableObjectType::None;
        selectedObjectIndex = -1;
    }
    handle.DirtyVariable("connectors");
    syncStationPartConnectorTextures();
    handle.DirtyVariable("textures");
    syncListSelectionToRml();
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
        fixSelectionAfterErase(SelectableObjectType::Connector, i);
        handle.DirtyVariable("connectors");
        syncStationPartConnectorTextures();
        handle.DirtyVariable("textures");
        syncListSelectionToRml();
    }
}

void ModdingTools::syncStationPartConnectorTextures()
{
    if (activeMode != ModdingToolsMode::StationPart)
    {
        return;
    }
    textures.erase(
        std::remove_if(textures.begin(),
                       textures.end(),
                       [](const TextureInfo& t)
                       { return toLowerCopy(t.name) == "station-connector"; }),
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
        t.rotVal = -c.rotDegVal;
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

    if (selectedObjectType == SelectableObjectType::Texture
        && (selectedObjectIndex < 0
            || selectedObjectIndex >= static_cast<int>(textures.size())))
    {
        selectedObjectType = SelectableObjectType::None;
        selectedObjectIndex = -1;
    }
    textureSizeAppliedForName.resize(textures.size());
    syncListSelectionToRml();
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

void ModdingTools::refreshPerRowTextureNameSuggestions()
{
    if (activeMode != ModdingToolsMode::Hull
        && activeMode != ModdingToolsMode::StationPart)
    {
        return;
    }
    bool anyChange = false;
    for (size_t i = 0; i < textures.size(); ++i)
    {
        TextureInfo& tex = textures[i];
        if (!textureNameUsesBoundsBleed(tex.name))
        {
            if (!tex.nameSuggestions.empty())
            {
                tex.nameSuggestions.clear();
                anyChange = true;
            }
            continue;
        }
        const string query = toLowerCopy(tex.name);
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
        if (filtered.size() != tex.nameSuggestions.size()
            || !std::equal(
                filtered.begin(), filtered.end(), tex.nameSuggestions.begin()))
        {
            tex.nameSuggestions = std::move(filtered);
            anyChange = true;
        }
    }
    if (anyChange && rmlModel_)
    {
        rmlModel_.DirtyVariable("textures");
    }
}

void ModdingTools::refreshNewTexturePickerSuggestions()
{
    const string query = toLowerCopy(newTexturePickerName);
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
    if (filteredNewTexturePickerNames != filtered)
    {
        filteredNewTexturePickerNames = std::move(filtered);
        rmlModel_.DirtyVariable("filteredNewTexturePickerNames");
    }
}

void ModdingTools::resetNewTexturePickerState()
{
    newTexturePickerName.clear();
    filteredNewTexturePickerNames.clear();
    if (rmlModel_)
    {
        rmlModel_.DirtyVariable("newTexturePickerName");
        rmlModel_.DirtyVariable("filteredNewTexturePickerNames");
    }
}

void ModdingTools::syncTextureSizesFromNames(gfx::RenderEngine& renderer)
{
    if (activeMode != ModdingToolsMode::Hull
        && activeMode != ModdingToolsMode::StationPart)
    {
        return;
    }
    while (textureSizeAppliedForName.size() < textures.size())
    {
        textureSizeAppliedForName.push_back(string());
    }
    if (textureSizeAppliedForName.size() > textures.size())
    {
        textureSizeAppliedForName.resize(textures.size());
    }
    bool dirty = false;
    for (size_t i = 0; i < textures.size(); ++i)
    {
        TextureInfo& tex = textures[i];
        if (!textureNameUsesBoundsBleed(tex.name))
        {
            textureSizeAppliedForName[i] = tex.name;
            continue;
        }
        if (tex.name == textureSizeAppliedForName[i])
        {
            continue;
        }
        textureSizeAppliedForName[i] = tex.name;
        if (tex.name.empty())
        {
            continue;
        }
        glm::vec2 texSizePx;
        if (renderer.getTexturePixelSize(tex.name, texSizePx))
        {
            tex.sizeXVal = kTexturePixelToWorld * texSizePx.x;
            tex.sizeYVal = kTexturePixelToWorld * texSizePx.y;
            floatToString(tex.sizeXVal, tex.sizeX, 2);
            floatToString(tex.sizeYVal, tex.sizeY, 2);
            dirty = true;
        }
    }
    if (dirty && rmlModel_)
    {
        rmlModel_.DirtyVariable("textures");
    }
}

TextureInfo ModdingTools::makeNewTextureEntry()
{
    TextureInfo texture;
    texture.zIndexVal = defaultTextureZForMode(activeMode);
    intToString(texture.zIndexVal, texture.zIndex);
    if ((activeMode == ModdingToolsMode::Hull
         || activeMode == ModdingToolsMode::StationPart)
        && !textures.empty())
    {
        const TextureInfo* lowest = &textures.front();
        for (const auto& t : textures)
        {
            if (t.posYVal > lowest->posYVal)
            {
                lowest = &t;
            }
        }
        texture.posXVal = lowest->posXVal;
        texture.posYVal =
            lowest->posYVal + 0.5f * (lowest->sizeYVal + texture.sizeYVal);
        floatToString(texture.posXVal, texture.posX, 2);
        floatToString(texture.posYVal, texture.posY, 2);
    }
    return texture;
}

void ModdingTools::draw(gfx::RenderEngine& renderer)
{
    const std::vector<string> currentTextureNames = renderer.getTextureNames();
    if (renderTextureNames != currentTextureNames)
    {
        renderTextureNames = currentTextureNames;
        rmlModel_.DirtyVariable("renderTextureNames");
    }

    parseEditorNumericFields();
    refreshPerRowTextureNameSuggestions();
    refreshNewTexturePickerSuggestions();
    syncTextureSizesFromNames(renderer);

    const gfx::ShaderHandle blueprintGrid =
        renderer.getShaderHandle("blueprintgrid");
    if (blueprintGrid.isValid())
    {
        constexpr float kGridCellWorld = 10.0f;
        constexpr float kMajorEveryCells = 5.0f;
        renderer.drawBlueprintGridBackground(
            0, blueprintGrid, kGridCellWorld, kMajorEveryCells);
    }
    if (activeMode == ModdingToolsMode::Hull
        || activeMode == ModdingToolsMode::StationPart)
    {
        constexpr float kWorldAxesHalfExtent = 10000.0f;
        constexpr float kWorldAxesZ = 100.0f;
        const float zoom = renderer.getWorldZoom();
        const float axisThickness =
            zoom > 1e-6f ? (2.0f / zoom) : 2.0f;
        /** 0.5 alpha (A=0x80). X axis y=0 green; Y axis x=0 red; ABGR. */
        constexpr uint32_t kAxisGreenAbgr = 0x8000ff00u;
        constexpr uint32_t kAxisRedAbgr = 0x800000ffu;
        renderer.drawLine(glm::vec2(-kWorldAxesHalfExtent, 0.0f),
                            glm::vec2(kWorldAxesHalfExtent, 0.0f),
                            kAxisGreenAbgr,
                            axisThickness,
                            kWorldAxesZ,
                            0);
        renderer.drawLine(glm::vec2(0.0f, -kWorldAxesHalfExtent),
                            glm::vec2(0.0f, kWorldAxesHalfExtent),
                            kAxisRedAbgr,
                            axisThickness,
                            kWorldAxesZ,
                            0);
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
    for (size_t i = 0; i < textures.size(); ++i)
    {
        auto& texture = textures[i];
        const gfx::TextureHandle texHandle =
            renderer.getTextureHandle(texture.name);
        const bool sel = selectedObjectType == SelectableObjectType::Texture
                         && selectedObjectIndex == static_cast<int>(i);
        const glm::vec2 drawSize = moddingTextureLogicalSizeToAsset(
            texture.sizeXVal,
            texture.sizeYVal,
            textureNameUsesBoundsBleed(texture.name));
        // Match Model::drawTextures / drawTexRect: body rot 0 → pass -texture.rot
        renderer.drawTexRect(glm::vec2(texture.posXVal, texture.posYVal),
                             drawSize,
                             texHandle,
                             -smath::degToRad(texture.rotVal),
                             tintIfSelected(0xffffffff, sel),
                             texture.zIndexVal / 100.0f,
                             0);
    }
}

void ModdingTools::drawSlots(gfx::RenderEngine& renderer)
{
    for (size_t i = 0; i < slots.size(); ++i)
    {
        const SlotInfo& slot = slots[i];
        const bool sel = selectedObjectType == SelectableObjectType::Slot
                         && selectedObjectIndex == static_cast<int>(i);
        switch (slot.slotTypeVal)
        {
            case gobj::ModuleSlotType::RoofS_Common:
            case gobj::ModuleSlotType::RoofM_Common:
            case gobj::ModuleSlotType::RoofL_Common:
                drawRoofSlot(renderer, slot, sel);
                break;
            case gobj::ModuleSlotType::ThrusterMainS_Common:
            case gobj::ModuleSlotType::ThrusterMainM_Common:
            case gobj::ModuleSlotType::ThrusterMainL_Common:
                drawThrusterMainSlot(renderer, slot, sel);
                break;
            case gobj::ModuleSlotType::ThrusterManeuverS_Common:
            case gobj::ModuleSlotType::ThrusterManeuverM_Common:
            case gobj::ModuleSlotType::ThrusterManeuverL_Common:
                drawThrusterManeuverSlot(renderer, slot, sel);
                break;
            case gobj::ModuleSlotType::InternalS_Common:
            case gobj::ModuleSlotType::InternalM_Common:
            case gobj::ModuleSlotType::InternalL_Common:
                drawInternalSlot(renderer, slot, sel);
                break;
            case gobj::ModuleSlotType::BayS_Common:
            case gobj::ModuleSlotType::BayM_Common:
            case gobj::ModuleSlotType::BayL_Common:
                drawBaySlot(renderer, slot, sel);
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

    for (size_t i = 0; i < collider.size(); ++i)
    {
        const auto& vertex = collider[i];
        const bool sel =
            selectedObjectType == SelectableObjectType::ColliderVertex
            && selectedObjectIndex == static_cast<int>(i);
        renderer.drawEllipse(glm::vec2(vertex.xVal, vertex.yVal),
                             dotRadius,
                             tintIfSelected(0xffffffff, sel),
                             0.0f,
                             0.0f,
                             0.0f,
                             0);
    }
    for (size_t i = 1; i < collider.size(); ++i)
    {
        const bool selSeg =
            selectedObjectType == SelectableObjectType::ColliderVertex
            && (selectedObjectIndex == static_cast<int>(i)
                || selectedObjectIndex == static_cast<int>(i - 1));
        renderer.drawLine(glm::vec2(collider[i - 1].xVal, collider[i - 1].yVal),
                          glm::vec2(collider[i].xVal, collider[i].yVal),
                          tintIfSelected(0xffffffff, selSeg),
                          lineWidth,
                          0.0f,
                          0);
    }
}

void ModdingTools::drawConnectors(gfx::RenderEngine& renderer)
{
    const float zoom = renderer.getWorldZoom();
    const glm::vec2 dotR(4.0f / zoom, 4.0f / zoom);
    const float lineW = 1.0f / zoom;
    for (size_t i = 0; i < connectors.size(); ++i)
    {
        const auto& c = connectors[i];
        const bool sel = selectedObjectType == SelectableObjectType::Connector
                         && selectedObjectIndex == static_cast<int>(i);
        const glm::vec2 p(c.posXVal, c.posYVal);
        renderer.drawEllipse(
            p, dotR, tintIfSelected(0xffffff00, sel), 0.0f, 0.0f, 0.0f, 0);

        const float rad = smath::degToRad(c.rotDegVal);
        const vec2 offset =
            -smath::rotateVec2(vec2(0.0f, gobj::kConnectorHeight / 2.0f), rad);
        renderer.drawLine(p,
                          p - 5.0f * offset,
                          tintIfSelected(0xffffff00, sel),
                          lineW,
                          0.0f,
                          0);
    }
}

void ModdingTools::drawRoofSlot(gfx::RenderEngine& renderer,
                                const SlotInfo& slot,
                                bool selected)
{
    float rot = smath::degToRad(slot.rotVal);
    const float s = gobj::SlotSize[static_cast<size_t>(slot.slotTypeVal)];
    const uint32_t col = tintIfSelected(0x800000ff, selected);
    renderer.drawEllipse(glm::vec2(slot.posXVal, slot.posYVal),
                         glm::vec2(s, s),
                         col,
                         0.0f,
                         rot,
                         slot.zIndexVal / 100.0f,
                         0);
    vec2 offs = smath::rotateVec2(glm::vec2(0.0f, s), rot);
    renderer.drawRectangle(glm::vec2(slot.posXVal, slot.posYVal) + offs,
                           glm::vec2(s * 0.3f, s),
                           col,
                           0.0f,
                           rot,
                           slot.zIndexVal / 100.0f,
                           0);
}

void ModdingTools::drawThrusterMainSlot(gfx::RenderEngine& renderer,
                                        const SlotInfo& slot,
                                        bool selected)
{
    const float s = gobj::SlotSize[static_cast<size_t>(slot.slotTypeVal)];
    float rot = smath::degToRad(slot.rotVal);
    const uint32_t colMain = tintIfSelected(0xff100000, selected);
    const uint32_t colSub = tintIfSelected(0x300000ff, selected);
    renderer.drawRectangle(glm::vec2(slot.posXVal, slot.posYVal),
                           glm::vec2(s, s * 2.0f),
                           colMain,
                           0.0f,
                           rot,
                           slot.zIndexVal / 100.0f,
                           0);
    vec2 offs = smath::rotateVec2(glm::vec2(0.0f, s * 2.0f), rot);
    renderer.drawRectangle(glm::vec2(slot.posXVal, slot.posYVal) - offs,
                           glm::vec2(s * 0.6f, s * 2.0f),
                           colSub,
                           0.0f,
                           rot,
                           slot.zIndexVal / 100.0f,
                           0);
}

void ModdingTools::drawThrusterManeuverSlot(gfx::RenderEngine& renderer,
                                            const SlotInfo& slot,
                                            bool selected)
{
    const float s = gobj::SlotSize[static_cast<size_t>(slot.slotTypeVal)];
    float rot = smath::degToRad(slot.rotVal);
    const uint32_t colMain = tintIfSelected(0xff100000, selected);
    const uint32_t colSub = tintIfSelected(0x300000ff, selected);
    renderer.drawRectangle(glm::vec2(slot.posXVal, slot.posYVal),
                           glm::vec2(s, s * 0.5f),
                           colMain,
                           0.0f,
                           rot,
                           slot.zIndexVal / 100.0f,
                           0);
    vec2 offs = smath::rotateVec2(glm::vec2(0.0f, s * 0.5f), rot);
    renderer.drawRectangle(glm::vec2(slot.posXVal, slot.posYVal) - offs,
                           glm::vec2(s * 0.6f, s * 0.5f),
                           colSub,
                           0.0f,
                           rot,
                           slot.zIndexVal / 100.0f,
                           0);
}

void ModdingTools::drawInternalSlot(gfx::RenderEngine& renderer,
                                    const SlotInfo& slot,
                                    bool selected)
{
    const float s = gobj::SlotSize[static_cast<size_t>(slot.slotTypeVal)];
    float rot = smath::degToRad(slot.rotVal);
    renderer.drawRectangle(glm::vec2(slot.posXVal, slot.posYVal),
                           glm::vec2(s, s),
                           tintIfSelected(0xff44ff44, selected),
                           0.0f,
                           rot,
                           slot.zIndexVal / 100.0f,
                           0);
}

void ModdingTools::drawBaySlot(gfx::RenderEngine& renderer,
                               const SlotInfo& slot,
                               bool selected)
{
    const float s = gobj::SlotSize[static_cast<size_t>(slot.slotTypeVal)];
    float rot = smath::degToRad(slot.rotVal);
    renderer.drawRectangle(glm::vec2(slot.posXVal, slot.posYVal),
                           glm::vec2(s, s * 0.2f),
                           tintIfSelected(0xffff8800, selected),
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
        const bool yamlTextureBoundsBleed =
            root[kYamlTextureBoundsBleedKey].IsDefined()
            && root[kYamlTextureBoundsBleedKey].as<bool>();

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
                const bool bleedThis = yamlTextureBoundsBleed
                                       && textureNameUsesBoundsBleed(t.name);
                const glm::vec2 logical =
                    moddingTextureAssetSizeToLogical(sx, sy, bleedThis);
                t.sizeXVal = logical.x;
                t.sizeYVal = logical.y;
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

    resetNewTexturePickerState();
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
        const bool yamlTextureBoundsBleed =
            root[kYamlTextureBoundsBleedKey].IsDefined()
            && root[kYamlTextureBoundsBleedKey].as<bool>();

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
                const bool bleedThis = yamlTextureBoundsBleed
                                       && textureNameUsesBoundsBleed(t.name);
                const glm::vec2 logical =
                    moddingTextureAssetSizeToLogical(sx, sy, bleedThis);
                t.sizeXVal = logical.x;
                t.sizeYVal = logical.y;
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

    resetNewTexturePickerState();
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
        const glm::vec2 assetSize = moddingTextureLogicalSizeToAsset(
            t.sizeXVal, t.sizeYVal, textureNameUsesBoundsBleed(t.name));
        entry["bounds"].push_back(t.posXVal);
        entry["bounds"].push_back(t.posYVal);
        entry["bounds"].push_back(assetSize.x);
        entry["bounds"].push_back(assetSize.y);
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
    root[kYamlTextureBoundsBleedKey] = true;
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
        const glm::vec2 assetSize = moddingTextureLogicalSizeToAsset(
            t.sizeXVal, t.sizeYVal, textureNameUsesBoundsBleed(t.name));
        entry["bounds"].push_back(t.posXVal);
        entry["bounds"].push_back(t.posYVal);
        entry["bounds"].push_back(assetSize.x);
        entry["bounds"].push_back(assetSize.y);
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
    root[kYamlTextureBoundsBleedKey] = true;
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
