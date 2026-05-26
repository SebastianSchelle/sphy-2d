#include "modding-tools.hpp"
#include "texture.hpp"
#include <algorithm>
#include <bimg/bimg.h>
#include <bimg/decode.h>
#include <bx/allocator.h>
#include <bx/error.h>
#include <cctype>
#include <cmath>
#include <fstream>
#include <magic_enum/magic_enum.hpp>
#include <optional>
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
constexpr float kTexturePixelToWorld = 0.15f;
constexpr float kModdingPasteNudgeWorld = 0.5f;
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

/** Editor displays metric tons; game/libs store mass × this factor. */
constexpr float kModdingGameMassPerTon = 1000.0f;
/** Editor displays k(t·m²); game/libs store inertia × this factor. */
constexpr float kModdingGameInertiaPerK = 1000.0f;
/** Editor displays MN; game/libs store thrust (N) × this factor. */
constexpr float kModdingGameThrustPerMN = 1.0e6f;
/** Editor displays kN·m; game/libs store torque (N·m) × this factor. */
constexpr float kModdingGameTorquePerKNm = 1000.0f;

void parseModdingMassTonsString(const string& tonsStr, float& gameMassOut)
{
    float tons = 0.0f;
    tryParseFloat(tonsStr, tons);
    gameMassOut = tons * kModdingGameMassPerTon;
}

void formatModdingMassTonsString(float gameMass, string& tonsStr)
{
    floatToString(gameMass / kModdingGameMassPerTon, tonsStr, 2);
}

void parseModdingInertiaKString(const string& inertiaKStr,
                                float& gameInertiaOut)
{
    float inertiaK = 0.0f;
    tryParseFloat(inertiaKStr, inertiaK);
    gameInertiaOut = inertiaK * kModdingGameInertiaPerK;
}

void formatModdingInertiaKString(float gameInertia, string& inertiaKStr)
{
    floatToString(gameInertia / kModdingGameInertiaPerK, inertiaKStr, 2);
}

void parseModdingThrustMNString(const string& thrustMNStr, float& gameThrustOut)
{
    float thrustMN = 0.0f;
    tryParseFloat(thrustMNStr, thrustMN);
    gameThrustOut = thrustMN * kModdingGameThrustPerMN;
}

void formatModdingThrustMNString(float gameThrust, string& thrustMNStr)
{
    floatToString(gameThrust / kModdingGameThrustPerMN, thrustMNStr, 2);
}

void parseModdingTorqueKNmString(const string& torqueKNmStr,
                                 float& gameTorqueOut)
{
    float torqueKNm = 0.0f;
    tryParseFloat(torqueKNmStr, torqueKNm);
    gameTorqueOut = torqueKNm * kModdingGameTorquePerKNm;
}

void formatModdingTorqueKNmString(float gameTorque, string& torqueKNmStr)
{
    floatToString(gameTorque / kModdingGameTorquePerKNm, torqueKNmStr, 2);
}

constexpr const char* kModuleStorageVolumeYamlKeys[static_cast<size_t>(
    gobj::StorageType::NumStorageTypes)] = {
    "volume-container-s",
    "volume-container-l",
    "volume-tank",
    "volume-bulk",
};

constexpr const char* kStationStorageCapYamlKeys[static_cast<size_t>(
    gobj::StorageType::NumStorageTypes)] = {
    "cap-container-s",
    "cap-container-l",
    "cap-tank",
    "cap-bulk",
};

constexpr const char* kTurretTypeYamlKey = "turret-type";
constexpr const char* kTurretDamageTypeYamlKey = "damage-type";
constexpr const char* kTurretNumBarrelsYamlKey = "num-barrels";
constexpr const char* kTurretExitsYamlKey = "barrel-exits";

void parseStorageVolumesInfoStrings(StorageVolumesInfo& volumes)
{
    tryParseFloat(volumes.containerS, volumes.containerSVal);
    tryParseFloat(volumes.containerL, volumes.containerLVal);
    tryParseFloat(volumes.tank, volumes.tankVal);
    tryParseFloat(volumes.bulk, volumes.bulkVal);
    floatToString(volumes.containerSVal, volumes.containerS, 2);
    floatToString(volumes.containerLVal, volumes.containerL, 2);
    floatToString(volumes.tankVal, volumes.tank, 2);
    floatToString(volumes.bulkVal, volumes.bulk, 2);
}

void loadModuleStorageVolumesFromYaml(const YAML::Node& dataNode,
                                      StorageVolumesInfo& volumes)
{
    volumes = StorageVolumesInfo{};
    if (!dataNode || !dataNode.IsMap())
    {
        return;
    }

    float* vals[] = {&volumes.containerSVal,
                     &volumes.containerLVal,
                     &volumes.tankVal,
                     &volumes.bulkVal};
    string* strs[] = {
        &volumes.containerS, &volumes.containerL, &volumes.tank, &volumes.bulk};

    bool anyPerType = false;
    for (size_t i = 0;
         i < static_cast<size_t>(gobj::StorageType::NumStorageTypes);
         ++i)
    {
        if (dataNode[kModuleStorageVolumeYamlKeys[i]])
        {
            anyPerType = true;
            *vals[i] = dataNode[kModuleStorageVolumeYamlKeys[i]].as<float>();
            floatToString(*vals[i], *strs[i], 2);
        }
    }

    if (!anyPerType && dataNode["volume"])
    {
        const float legacy = dataNode["volume"].as<float>();
        for (size_t i = 0;
             i < static_cast<size_t>(gobj::StorageType::NumStorageTypes);
             ++i)
        {
            *vals[i] = legacy;
            floatToString(*vals[i], *strs[i], 2);
        }
    }
}

void writeModuleStorageVolumesToYaml(YAML::Node& dataNode,
                                     const StorageVolumesInfo& volumes)
{
    dataNode["volume-container-s"] = volumes.containerSVal;
    dataNode["volume-container-l"] = volumes.containerLVal;
    dataNode["volume-tank"] = volumes.tankVal;
    dataNode["volume-bulk"] = volumes.bulkVal;
}

void loadStationStorageVolumesFromYaml(const YAML::Node& dataNode,
                                       StorageVolumesInfo& volumes)
{
    volumes = StorageVolumesInfo{};
    if (!dataNode || !dataNode.IsMap())
    {
        return;
    }

    float* vals[] = {&volumes.containerSVal,
                     &volumes.containerLVal,
                     &volumes.tankVal,
                     &volumes.bulkVal};
    string* strs[] = {
        &volumes.containerS, &volumes.containerL, &volumes.tank, &volumes.bulk};

    bool anyPerType = false;
    for (size_t i = 0;
         i < static_cast<size_t>(gobj::StorageType::NumStorageTypes);
         ++i)
    {
        if (dataNode[kStationStorageCapYamlKeys[i]])
        {
            anyPerType = true;
            *vals[i] = dataNode[kStationStorageCapYamlKeys[i]].as<float>();
            floatToString(*vals[i], *strs[i], 2);
        }
    }

    if (!anyPerType && dataNode["volume"])
    {
        const float legacy = dataNode["volume"].as<float>();
        for (size_t i = 0;
             i < static_cast<size_t>(gobj::StorageType::NumStorageTypes);
             ++i)
        {
            *vals[i] = legacy;
            floatToString(*vals[i], *strs[i], 2);
        }
    }
}

void writeStationStorageVolumesToYaml(YAML::Node& dataNode,
                                      const StorageVolumesInfo& volumes)
{
    dataNode["cap-container-s"] = volumes.containerSVal;
    dataNode["cap-container-l"] = volumes.containerLVal;
    dataNode["cap-tank"] = volumes.tankVal;
    dataNode["cap-bulk"] = volumes.bulkVal;
}

constexpr uint32_t kColliderDotSize = 3.0f;
constexpr uint32_t kColliderColor = 0xff40c0ffu;
constexpr float kColliderPickR = 4.0f;
constexpr float kConnectorPickR = 4.0f;

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
    if (angleErr > 45.0f)
    {
        angleErr -= 90.0f;
    }
    else if (angleErr < -45.0f)
    {
        angleErr += 90.0f;
    }
    return selectedDeg + angleErr;
}
constexpr uint8_t kColliderAlphaThreshold = 128;
constexpr int kColliderImageSampleStride = 4;

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

int slotDrawZ(const SlotInfo& slot)
{
    const size_t i = static_cast<size_t>(slot.slotTypeVal);
    if (i >= static_cast<size_t>(gobj::ModuleSlotType::NumSlotTypes))
    {
        return 0;
    }
    return gfx::RenderEngine::zIdxShipHull + gobj::ModuleSlotZOffset[i];
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

glm::vec2 moddingTextureDrawSize(const TextureInfo& tex);

/** Radians passed to drawTexRect / texrect VS (negated editor degrees). */
float moddingTextureRotRad(const TextureInfo& tex)
{
    return -smath::degToRad(tex.rotVal);
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

bool hitDisc(const glm::vec2& p, const glm::vec2& center, float radius)
{
    const glm::vec2 d = p - center;
    return glm::dot(d, d) <= radius * radius;
}

const char* exampleSlotTextureName(gobj::ModuleSlotType slotType)
{
    switch (slotType)
    {
        case gobj::ModuleSlotType::ThrusterMainS_Common:
            return "ex-thm-s";
        case gobj::ModuleSlotType::ThrusterMainM_Common:
            return "ex-thm-m";
        case gobj::ModuleSlotType::ThrusterMainL_Common:
            return "ex-thm-l";
        case gobj::ModuleSlotType::ThrusterManeuverS_Common:
            return "ex-ths-s";
        case gobj::ModuleSlotType::ThrusterManeuverM_Common:
            return "ex-ths-m";
        case gobj::ModuleSlotType::ThrusterManeuverL_Common:
            return "ex-ths-l";
        case gobj::ModuleSlotType::InternalS_Common:
            return "ex-int-s";
        case gobj::ModuleSlotType::InternalM_Common:
            return "ex-int-m";
        case gobj::ModuleSlotType::InternalL_Common:
            return "ex-int-l";
        case gobj::ModuleSlotType::RoofS_Common:
            return "ex-roof-s";
        case gobj::ModuleSlotType::RoofM_Common:
            return "ex-roof-m";
        case gobj::ModuleSlotType::RoofL_Common:
            return "ex-roof-l";
        case gobj::ModuleSlotType::BayS_Common:
            return "ex-bay-s";
        case gobj::ModuleSlotType::BayM_Common:
            return "ex-bay-m";
        case gobj::ModuleSlotType::BayL_Common:
            return "ex-bay-l";
        default:
            return nullptr;
    }
}


bool exampleTextureWorldSize(gfx::RenderEngine& renderer,
                             const char* textureName,
                             glm::vec2& worldSizeOut)
{
    if (textureName == nullptr || textureName[0] == '\0')
    {
        return false;
    }
    glm::vec2 texSizePx;
    if (!renderer.getTexturePixelSize(textureName, texSizePx))
    {
        return false;
    }
    worldSizeOut = kTexturePixelToWorld * texSizePx;
    return true;
}

bool hitSlot(const glm::vec2& p,
             const SlotInfo& slot,
             gfx::RenderEngine* renderer)
{
    if (renderer == nullptr)
    {
        return false;
    }
    const char* texName = exampleSlotTextureName(slot.slotTypeVal);
    glm::vec2 size;
    if (!exampleTextureWorldSize(*renderer, texName, size))
    {
        return false;
    }
    return hitRotatedRect(p,
                          glm::vec2(slot.posXVal, slot.posYVal),
                          size,
                          smath::degToRad(slot.rotVal));
}

void drawExampleSlotTexture(gfx::RenderEngine& renderer,
                            const SlotInfo& slot,
                            const char* textureName,
                            uint32_t tintAbgr,
                            bool selected)
{
    const gfx::TextureHandle texHandle = renderer.getTextureHandle(textureName);
    if (!texHandle.isValid())
    {
        return;
    }
    glm::vec2 size;
    if (!exampleTextureWorldSize(renderer, textureName, size))
    {
        return;
    }
    renderer.queueTexRect(glm::vec2(slot.posXVal, slot.posYVal),
                          size,
                          texHandle,
                          smath::degToRad(slot.rotVal),
                          slotDrawZ(slot),
                          tintIfSelected(tintAbgr, selected),
                          0,
                          glm::vec2(0.0f, 0.0f),
                          glm::vec2(1.0f, 1.0f));
}

bool hitTexture(const glm::vec2& p, const TextureInfo& tex)
{
    return hitRotatedRect(p,
                          glm::vec2(tex.posXVal, tex.posYVal),
                          moddingTextureDrawSize(tex),
                          moddingTextureRotRad(tex));
}

/** Squared distance from p to texture OBB (0 when inside). Uses draw size +
 * shader rot. */
float distSqPointToModdingTexture(const glm::vec2& p, const TextureInfo& tex)
{
    const glm::vec2 drawSize = moddingTextureDrawSize(tex);
    const float theta = moddingTextureRotRad(tex);
    const float c = std::cos(theta);
    const float s = std::sin(theta);
    const glm::vec2 d(p.x - tex.posXVal, p.y - tex.posYVal);
    const float lx = d.x * c + d.y * s;
    const float ly = -d.x * s + d.y * c;
    const float hw = drawSize.x * 0.5f;
    const float hh = drawSize.y * 0.5f;
    const float dx = std::max(0.0f, std::fabs(lx) - hw);
    const float dy = std::max(0.0f, std::fabs(ly) - hh);
    return dx * dx + dy * dy;
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
    const glm::vec2 anchorDraw = moddingTextureDrawSize(anchor);
    const float theta = moddingTextureRotRad(anchor);
    const float c = std::cos(theta);
    const float s = std::sin(theta);
    const glm::vec2 d(worldPos.x - anchor.posXVal, worldPos.y - anchor.posYVal);
    const float lx = d.x * c + d.y * s;
    const float ly = -d.x * s + d.y * c;
    const float hw = anchorDraw.x * 0.5f;
    const float hh = anchorDraw.y * 0.5f;
    /** Clamp so clicks outside the rect still pick the nearest edge. */
    const float lxCl = std::clamp(lx, -hw, hw);
    const float lyCl = std::clamp(ly, -hh, hh);
    const float distLeft = lxCl + hw;
    const float distRight = hw - lxCl;
    const float distBottom = lyCl + hh;
    const float distTop = hh - lyCl;
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
    const float theta = moddingTextureRotRad(anchor);
    const float c = std::cos(theta);
    const float s = std::sin(theta);
    const glm::vec2 selectedDraw = moddingTextureDrawSize(selected);
    const glm::vec2 anchorDraw = moddingTextureDrawSize(anchor);
    const float w1 = selectedDraw.x;
    const float h1 = selectedDraw.y;
    const float w2 = anchorDraw.x;
    const float h2 = anchorDraw.y;
    /** Visual relative rotation (matches drawTexRect / texrect shader). */
    const float rotSel = -smath::degToRad(snappedRotDeg);
    const float rotAnc = moddingTextureRotRad(anchor);
    const float ds = rotSel - rotAnc;
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
    const glm::vec2 offsetWorld = smath::rotateVec2(glm::vec2(olx, oly), theta);
    selected.posXVal = anchor.posXVal + offsetWorld.x;
    selected.posYVal = anchor.posYVal + offsetWorld.y;
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

/** World-space width/height used for draw, hit-test, and shift-align (includes
 * bleed). */
glm::vec2 moddingTextureDrawSize(const TextureInfo& tex)
{
    return moddingTextureLogicalSizeToAsset(
        tex.sizeXVal, tex.sizeYVal, textureNameUsesBoundsBleed(tex.name));
}

void readTextureTileFields(const YAML::Node& texNode, TextureInfo& t)
{
    t.tileCntXVal = 1.0f;
    t.tileCntYVal = 1.0f;
    t.tileOffXVal = 0.0f;
    t.tileOffYVal = 0.0f;
    if (texNode["tileCount"] && texNode["tileCount"].IsSequence()
        && texNode["tileCount"].size() >= 2)
    {
        t.tileCntXVal = texNode["tileCount"][0].as<float>();
        t.tileCntYVal = texNode["tileCount"][1].as<float>();
    }
    if (texNode["tileOffset"] && texNode["tileOffset"].IsSequence()
        && texNode["tileOffset"].size() >= 2)
    {
        t.tileOffXVal = texNode["tileOffset"][0].as<float>();
        t.tileOffYVal = texNode["tileOffset"][1].as<float>();
    }
}

void writeTextureTileFields(YAML::Node& entry, const TextureInfo& t)
{
    entry["tileCount"] = YAML::Node(YAML::NodeType::Sequence);
    entry["tileCount"].push_back(t.tileCntXVal);
    entry["tileCount"].push_back(t.tileCntYVal);
    entry["tileOffset"] = YAML::Node(YAML::NodeType::Sequence);
    entry["tileOffset"].push_back(t.tileOffXVal);
    entry["tileOffset"].push_back(t.tileOffYVal);
}

void syncTextureTileStrings(TextureInfo& t)
{
    floatToString(t.tileCntXVal, t.tileCntX, 2);
    floatToString(t.tileCntYVal, t.tileCntY, 2);
    floatToString(t.tileOffXVal, t.tileOffX, 2);
    floatToString(t.tileOffYVal, t.tileOffY, 2);
    const bool hasTile = std::abs(t.tileCntXVal - 1.0f) > 1.0e-4f
                         || std::abs(t.tileCntYVal - 1.0f) > 1.0e-4f
                         || std::abs(t.tileOffXVal) > 1.0e-4f
                         || std::abs(t.tileOffYVal) > 1.0e-4f;
    if (!hasTile)
    {
        t.tileModifiers.clear();
        return;
    }
    t.tileModifiers = "tiles " + t.tileCntX + "×" + t.tileCntY + " · off "
                      + t.tileOffX + "," + t.tileOffY;
}

struct TextureYamlLoadOptions
{
    bool yamlTextureBoundsBleed = false;
    bool skipStationConnector = false;
};

TextureInfo textureInfoFromYamlNode(const YAML::Node& texNode,
                                    const TextureYamlLoadOptions& opts)
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
    const bool bleedThis =
        opts.yamlTextureBoundsBleed && textureNameUsesBoundsBleed(t.name);
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

    readTextureTileFields(texNode, t);
    syncTextureTileStrings(t);
    return t;
}

void loadTexturesFromRootBundle(const YAML::Node& root,
                                const string& texKey,
                                std::vector<TextureInfo>& out,
                                const TextureYamlLoadOptions& opts)
{
    const YAML::Node texRoot = root["textures"];
    if (texKey.empty() || !texRoot || !texRoot[texKey]
        || !texRoot[texKey]["textures"])
    {
        return;
    }
    for (const auto& texNode : texRoot[texKey]["textures"])
    {
        TextureInfo t = textureInfoFromYamlNode(texNode, opts);
        if (opts.skipStationConnector
            && toLowerCopy(t.name) == "station-connector")
        {
            continue;
        }
        out.push_back(std::move(t));
    }
}

YAML::Node textureInfoToYamlNode(const TextureInfo& t)
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
    writeTextureTileFields(entry, t);
    return entry;
}

YAML::Node buildTexturesYamlBundle(const std::vector<TextureInfo>& textures)
{
    YAML::Node texBundle;
    YAML::Node texList(YAML::NodeType::Sequence);
    for (const auto& t : textures)
    {
        texList.push_back(textureInfoToYamlNode(t));
    }
    texBundle["textures"] = texList;
    return texBundle;
}

void loadColliderVerticesFromEntry(const YAML::Node& colEntry,
                                   std::vector<ColliderVertex>& colliderOut,
                                   float& restitutionOut)
{
    if (colEntry["restitution"])
    {
        restitutionOut = colEntry["restitution"].as<float>();
    }
    const YAML::Node vertNode = colEntry["vertices"];
    if (!vertNode || !vertNode.IsSequence())
    {
        return;
    }
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
        colliderOut.push_back(std::move(v));
    }
}

void parseGeneralInfoNumericFields(GeneralInfo& info, float& hpOut)
{
    tryParseFloat(info.hp, hpOut);
    floatToString(hpOut, info.hp, 2);
    parseModdingMassTonsString(info.mass, info.massVal);
    formatModdingMassTonsString(info.massVal, info.mass);
    parseModdingTorqueKNmString(info.internalGyroTorque,
                                info.internalGyroTorqueVal);
    formatModdingTorqueKNmString(info.internalGyroTorqueVal,
                                 info.internalGyroTorque);
    parseStorageVolumesInfoStrings(info.storageVolumes);
}

void applyTurretNumBarrelsFromExits(ModuleInfo& info,
                                    const vector<TurretExitInfo>& exits)
{
    info.turretNumBarrelsVal = static_cast<int>(exits.size());
    intToString(info.turretNumBarrelsVal, info.turretNumBarrels);
}

void parseModuleInfoNumericFields(ModuleInfo& info)
{
    if (auto modType = magic_enum::enum_cast<gobj::ModuleType>(info.moduleType);
        modType.has_value())
    {
        info.moduleTypeVal = modType.value();
    }
    if (auto slotType =
            magic_enum::enum_cast<gobj::ModuleSlotType>(info.slotType);
        slotType.has_value())
    {
        info.slotTypeVal = slotType.value();
    }
    parseModdingMassTonsString(info.mass, info.massVal);
    formatModdingMassTonsString(info.massVal, info.mass);
    parseModdingThrustMNString(info.maxThrust, info.maxThrustVal);
    formatModdingThrustMNString(info.maxThrustVal, info.maxThrust);
    parseStorageVolumesInfoStrings(info.storageVolumes);
    if (auto shipClass =
            magic_enum::enum_cast<def::ShipClass>(info.hangarMaxShipClass);
        shipClass.has_value())
    {
        info.hangarMaxShipClassVal = shipClass.value();
    }
    tryParseFloat(info.hangarSpace, info.hangarSpaceVal);
    floatToString(info.hangarSpaceVal, info.hangarSpace, 2);
    if (auto turretType =
            magic_enum::enum_cast<def::TurretType>(info.turretType);
        turretType.has_value())
    {
        info.turretTypeVal = turretType.value();
    }
    else
    {
        info.turretTypeVal = def::TurretType::Projectile;
        info.turretType = string(magic_enum::enum_name(info.turretTypeVal));
    }
    if (auto damageType =
            magic_enum::enum_cast<def::DamageType>(info.turretDamageType);
        damageType.has_value())
    {
        info.turretDamageTypeVal = damageType.value();
    }
    else
    {
        info.turretDamageTypeVal =
            def::TurretTypeDefaultDamage[static_cast<size_t>(
                info.turretTypeVal)];
        info.turretDamageType =
            string(magic_enum::enum_name(info.turretDamageTypeVal));
    }
    tryParseFloat(info.turretProjDmg, info.turretProjDmgVal);
    tryParseFloat(info.turretExitSpeed, info.turretExitSpeedVal);
    tryParseFloat(info.turretLifetime, info.turretLifetimeVal);
    tryParseFloat(info.turretReloadTime, info.turretReloadTimeVal);
    tryParseFloat(info.turretDps, info.turretDpsVal);
    tryParseFloat(info.turretBeamWidth, info.turretBeamWidthVal);
    tryParseFloat(info.turretBeamLength, info.turretBeamLengthVal);
    tryParseFloat(info.turretArcAngle, info.turretArcAngleVal);
    tryParseFloat(info.turretArcLength, info.turretArcLengthVal);
    floatToString(info.turretProjDmgVal, info.turretProjDmg, 2);
    floatToString(info.turretExitSpeedVal, info.turretExitSpeed, 2);
    floatToString(info.turretLifetimeVal, info.turretLifetime, 2);
    floatToString(info.turretReloadTimeVal, info.turretReloadTime, 2);
    floatToString(info.turretDpsVal, info.turretDps, 2);
    floatToString(info.turretBeamWidthVal, info.turretBeamWidth, 2);
    floatToString(info.turretBeamLengthVal, info.turretBeamLength, 2);
    floatToString(info.turretArcAngleVal, info.turretArcAngle, 2);
    floatToString(info.turretArcLengthVal, info.turretArcLength, 2);
}

void writeModuleDataYaml(YAML::Node& dataNode, const ModuleInfo& info)
{
    switch (info.moduleTypeVal)
    {
        case gobj::ModuleType::MainThruster:
        case gobj::ModuleType::ManeuverThruster:
            dataNode["max-thrust"] = info.maxThrustVal;
            break;
        case gobj::ModuleType::Storage:
            writeModuleStorageVolumesToYaml(dataNode, info.storageVolumes);
            break;
        case gobj::ModuleType::Hangar:
            dataNode["max-ship-class"] = info.hangarMaxShipClass;
            dataNode["hangar-space"] = info.hangarSpaceVal;
            break;
        case gobj::ModuleType::Turret:
            dataNode[kTurretTypeYamlKey] = info.turretType;
            dataNode[kTurretDamageTypeYamlKey] = info.turretDamageType;
            dataNode[kTurretNumBarrelsYamlKey] = info.turretNumBarrelsVal;
            switch (info.turretTypeVal)
            {
                case def::TurretType::Laser:
                    dataNode["dps"] = info.turretDpsVal;
                    dataNode["beam-width"] = info.turretBeamWidthVal;
                    dataNode["beam-length"] = info.turretBeamLengthVal;
                    break;
                case def::TurretType::Arc:
                    dataNode["dps"] = info.turretDpsVal;
                    dataNode["arc-angle"] = info.turretArcAngleVal;
                    dataNode["arc-length"] = info.turretArcLengthVal;
                    break;
                case def::TurretType::Projectile:
                case def::TurretType::Railgun:
                case def::TurretType::Missile:
                case def::TurretType::Mining:
                default:
                    dataNode["proj-dmg"] = info.turretProjDmgVal;
                    dataNode["exit-speed"] = info.turretExitSpeedVal;
                    dataNode["lifetime"] = info.turretLifetimeVal;
                    dataNode["reload-time"] = info.turretReloadTimeVal;
                    break;
            }
            break;
        case gobj::ModuleType::None:
        default:
            break;
    }
}

string makeTextureSizeSyncKey(const TextureInfo& t)
{
    string tileCntXStr;
    string tileCntYStr;
    floatToString(t.tileCntXVal, tileCntXStr, 2);
    floatToString(t.tileCntYVal, tileCntYStr, 2);
    return t.name + "|" + tileCntXStr + "x" + tileCntYStr;
}

bool parseTextureSizeSyncKeyTileCounts(const string& key,
                                       float& tileCntX,
                                       float& tileCntY)
{
    const size_t sep = key.find('|');
    if (sep == string::npos)
    {
        return false;
    }
    const string tilePart = key.substr(sep + 1);
    const size_t mid = tilePart.find('x');
    if (mid == string::npos)
    {
        return false;
    }
    try
    {
        tileCntX = std::stof(tilePart.substr(0, mid));
        tileCntY = std::stof(tilePart.substr(mid + 1));
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool tryTextureBaseTileSize(gfx::RenderEngine& renderer,
                            const TextureInfo& tex,
                            glm::vec2& baseOut)
{
    if (toLowerCopy(tex.name) == "station-connector")
    {
        baseOut = {gobj::kConnectorWidth, gobj::kConnectorHeight};
        return true;
    }
    if (!textureNameUsesBoundsBleed(tex.name))
    {
        return false;
    }
    glm::vec2 texSizePx;
    if (!renderer.getTexturePixelSize(tex.name, texSizePx))
    {
        return false;
    }
    baseOut = kTexturePixelToWorld * texSizePx;
    return true;
}

void setTextureSizeFromBaseTile(TextureInfo& tex, const glm::vec2& baseTile)
{
    tex.sizeXVal = baseTile.x * tex.tileCntXVal;
    tex.sizeYVal = baseTile.y * tex.tileCntYVal;
    floatToString(tex.sizeXVal, tex.sizeX, 2);
    floatToString(tex.sizeYVal, tex.sizeY, 2);
}

struct RgbaImage
{
    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
};

std::optional<RgbaImage> loadRgbaImageFromPath(const std::string& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        return std::nullopt;
    }
    const auto sizef = static_cast<uint32_t>(file.tellg());
    if (sizef == 0)
    {
        return std::nullopt;
    }
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(sizef);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), sizef))
    {
        return std::nullopt;
    }

    bx::DefaultAllocator alloc;
    bx::Error err;
    bimg::ImageContainer* image = bimg::imageParse(
        &alloc, buffer.data(), sizef, bimg::TextureFormat::RGBA8, &err);
    if (!image || !err.isOk() || image->m_data == nullptr)
    {
        if (image)
        {
            bimg::imageFree(image);
        }
        return std::nullopt;
    }

    RgbaImage out;
    out.width = static_cast<int>(image->m_width);
    out.height = static_cast<int>(image->m_height);
    const size_t byteCount =
        static_cast<size_t>(out.width) * static_cast<size_t>(out.height) * 4u;
    const uint8_t* src = static_cast<const uint8_t*>(image->m_data);
    out.pixels.assign(src, src + byteCount);
    bimg::imageFree(image);
    return out;
}

glm::vec2 rotateLocalToWorld(const glm::vec2& local, float rotRad)
{
    const float c = std::cos(rotRad);
    const float s = std::sin(rotRad);
    return {local.x * c - local.y * s, local.x * s + local.y * c};
}

void appendOpaqueTextureSamples(const TextureInfo& tex,
                                const RgbaImage& image,
                                std::vector<glm::vec2>& outWorldPts)
{
    if (image.width <= 0 || image.height <= 0 || image.pixels.empty())
    {
        return;
    }

    const glm::vec2 drawSize = moddingTextureDrawSize(tex);
    const float rotRad = moddingTextureRotRad(tex);
    const glm::vec2 center(tex.posXVal, tex.posYVal);
    const int tileCntX =
        std::max(1, static_cast<int>(std::round(tex.tileCntXVal)));
    const int tileCntY =
        std::max(1, static_cast<int>(std::round(tex.tileCntYVal)));
    const int tileOffX = static_cast<int>(std::round(tex.tileOffXVal));
    const int tileOffY = static_cast<int>(std::round(tex.tileOffYVal));
    const int stride = std::max(1, kColliderImageSampleStride);

    for (int py = 0; py < image.height; py += stride)
    {
        for (int px = 0; px < image.width; px += stride)
        {
            const size_t idx =
                (static_cast<size_t>(py) * static_cast<size_t>(image.width)
                 + static_cast<size_t>(px))
                * 4u;
            if (image.pixels[idx + 3] < kColliderAlphaThreshold)
            {
                continue;
            }
            const float u = (static_cast<float>(px) + 0.5f)
                            / static_cast<float>(image.width);
            const float v = (static_cast<float>(py) + 0.5f)
                            / static_cast<float>(image.height);
            for (int ty = 0; ty < tileCntY; ++ty)
            {
                for (int tx = 0; tx < tileCntX; ++tx)
                {
                    const float unitX = (static_cast<float>(tx) + u
                                         - static_cast<float>(tileOffX))
                                        / static_cast<float>(tileCntX);
                    const float unitY = (static_cast<float>(ty) + v
                                         - static_cast<float>(tileOffY))
                                        / static_cast<float>(tileCntY);
                    const glm::vec2 local((unitX - 0.5f) * drawSize.x,
                                          (unitY - 0.5f) * drawSize.y);
                    outWorldPts.push_back(center
                                          + rotateLocalToWorld(local, rotRad));
                }
            }
        }
    }
}

float cross2d(const glm::vec2& o, const glm::vec2& a, const glm::vec2& b)
{
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

std::vector<glm::vec2> convexHull(std::vector<glm::vec2> pts)
{
    if (pts.size() < 3)
    {
        return pts;
    }
    std::sort(pts.begin(),
              pts.end(),
              [](const glm::vec2& a, const glm::vec2& b)
              {
                  if (a.x != b.x)
                  {
                      return a.x < b.x;
                  }
                  return a.y < b.y;
              });
    pts.erase(std::unique(pts.begin(),
                          pts.end(),
                          [](const glm::vec2& a, const glm::vec2& b)
                          { return glm::dot(a - b, a - b) < 1e-8f; }),
              pts.end());
    if (pts.size() < 3)
    {
        return pts;
    }

    std::vector<glm::vec2> lower;
    for (const glm::vec2& p : pts)
    {
        while (lower.size() >= 2
               && cross2d(lower[lower.size() - 2], lower.back(), p) <= 0.0f)
        {
            lower.pop_back();
        }
        lower.push_back(p);
    }
    std::vector<glm::vec2> upper;
    for (auto it = pts.rbegin(); it != pts.rend(); ++it)
    {
        while (upper.size() >= 2
               && cross2d(upper[upper.size() - 2], upper.back(), *it) <= 0.0f)
        {
            upper.pop_back();
        }
        upper.push_back(*it);
    }
    lower.pop_back();
    upper.pop_back();
    lower.insert(lower.end(), upper.begin(), upper.end());
    return lower;
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
        activeTextureIndex = selectedObjectType == SelectableObjectType::Texture
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

bool ModdingTools::canEditObjectType(SelectableObjectType type) const
{
    switch (type)
    {
        case SelectableObjectType::Texture:
            return activeMode == ModdingToolsMode::Hull
                   || activeMode == ModdingToolsMode::Module
                   || activeMode == ModdingToolsMode::StationPart;
        case SelectableObjectType::Slot:
            return activeMode == ModdingToolsMode::Hull;
        case SelectableObjectType::ColliderVertex:
            return activeMode == ModdingToolsMode::Hull
                   || activeMode == ModdingToolsMode::StationPart;
        case SelectableObjectType::Connector:
            return activeMode == ModdingToolsMode::StationPart;
        case SelectableObjectType::TurretExit:
            return activeMode == ModdingToolsMode::Module
                   && moduleInfo.moduleTypeVal == gobj::ModuleType::Turret;
        default:
            return false;
    }
}

void ModdingTools::copySelectedToClipboard()
{
    if (!canEditObjectType(selectedObjectType) || selectedObjectIndex < 0)
    {
        return;
    }
    clipboard_.valid = false;
    clipboard_.type = selectedObjectType;
    switch (selectedObjectType)
    {
        case SelectableObjectType::Texture:
            if (selectedObjectIndex >= static_cast<int>(textures.size()))
            {
                return;
            }
            clipboard_.texture =
                textures[static_cast<size_t>(selectedObjectIndex)];
            clipboard_.texture.nameSuggestions.clear();
            clipboard_.texture.tileModifiers.clear();
            break;
        case SelectableObjectType::Slot:
            if (selectedObjectIndex >= static_cast<int>(slots.size()))
            {
                return;
            }
            clipboard_.slot = slots[static_cast<size_t>(selectedObjectIndex)];
            break;
        case SelectableObjectType::ColliderVertex:
            if (selectedObjectIndex >= static_cast<int>(collider.size()))
            {
                return;
            }
            clipboard_.colliderVertex =
                collider[static_cast<size_t>(selectedObjectIndex)];
            break;
        case SelectableObjectType::Connector:
            if (selectedObjectIndex >= static_cast<int>(connectors.size()))
            {
                return;
            }
            clipboard_.connector =
                connectors[static_cast<size_t>(selectedObjectIndex)];
            break;
        case SelectableObjectType::TurretExit:
            if (selectedObjectIndex >= static_cast<int>(turretExits.size()))
            {
                return;
            }
            clipboard_.colliderVertex.x =
                turretExits[static_cast<size_t>(selectedObjectIndex)].x;
            clipboard_.colliderVertex.y =
                turretExits[static_cast<size_t>(selectedObjectIndex)].y;
            clipboard_.colliderVertex.xVal =
                turretExits[static_cast<size_t>(selectedObjectIndex)].xVal;
            clipboard_.colliderVertex.yVal =
                turretExits[static_cast<size_t>(selectedObjectIndex)].yVal;
            break;
        default:
            return;
    }
    clipboard_.valid = true;
}

bool ModdingTools::pasteFromClipboard()
{
    if (!clipboard_.valid || !canEditObjectType(clipboard_.type) || !rmlModel_)
    {
        return false;
    }
    switch (clipboard_.type)
    {
        case SelectableObjectType::Texture:
        {
            TextureInfo t = clipboard_.texture;
            t.posXVal += kModdingPasteNudgeWorld;
            t.posYVal += kModdingPasteNudgeWorld;
            floatToString(t.posXVal, t.posX, 2);
            floatToString(t.posYVal, t.posY, 2);
            syncTextureTileStrings(t);
            textures.push_back(std::move(t));
            textureSizeAppliedForName.push_back(string());
            selectedObjectType = SelectableObjectType::Texture;
            selectedObjectIndex = static_cast<int>(textures.size()) - 1;
            rmlModel_.DirtyVariable("textures");
            break;
        }
        case SelectableObjectType::Slot:
        {
            SlotInfo s = clipboard_.slot;
            s.posXVal += kModdingPasteNudgeWorld;
            s.posYVal += kModdingPasteNudgeWorld;
            floatToString(s.posXVal, s.posX, 2);
            floatToString(s.posYVal, s.posY, 2);
            slots.push_back(std::move(s));
            selectedObjectType = SelectableObjectType::Slot;
            selectedObjectIndex = static_cast<int>(slots.size()) - 1;
            rmlModel_.DirtyVariable("slots");
            break;
        }
        case SelectableObjectType::ColliderVertex:
        {
            ColliderVertex v = clipboard_.colliderVertex;
            v.xVal += kModdingPasteNudgeWorld;
            v.yVal += kModdingPasteNudgeWorld;
            floatToString(v.xVal, v.x, 2);
            floatToString(v.yVal, v.y, 2);
            collider.push_back(std::move(v));
            selectedObjectType = SelectableObjectType::ColliderVertex;
            selectedObjectIndex = static_cast<int>(collider.size()) - 1;
            rmlModel_.DirtyVariable("collider");
            break;
        }
        case SelectableObjectType::Connector:
        {
            ConnectorInfo c = clipboard_.connector;
            c.posXVal += kModdingPasteNudgeWorld;
            c.posYVal += kModdingPasteNudgeWorld;
            floatToString(c.posXVal, c.posX, 2);
            floatToString(c.posYVal, c.posY, 2);
            connectors.push_back(std::move(c));
            syncStationPartConnectorTextures();
            rmlModel_.DirtyVariable("connectors");
            rmlModel_.DirtyVariable("textures");
            selectedObjectType = SelectableObjectType::Connector;
            selectedObjectIndex = static_cast<int>(connectors.size()) - 1;
            break;
        }
        case SelectableObjectType::TurretExit:
        {
            TurretExitInfo te;
            te.xVal = clipboard_.colliderVertex.xVal + kModdingPasteNudgeWorld;
            te.yVal = clipboard_.colliderVertex.yVal + kModdingPasteNudgeWorld;
            floatToString(te.xVal, te.x, 2);
            floatToString(te.yVal, te.y, 2);
            turretExits.push_back(std::move(te));
            rmlModel_.DirtyVariable("turretExits");
            selectedObjectType = SelectableObjectType::TurretExit;
            selectedObjectIndex = static_cast<int>(turretExits.size()) - 1;
            break;
        }
        default:
            return false;
    }
    switch (clipboard_.type)
    {
        case SelectableObjectType::Texture:
            clipboard_.texture.posXVal += kModdingPasteNudgeWorld;
            clipboard_.texture.posYVal += kModdingPasteNudgeWorld;
            floatToString(
                clipboard_.texture.posXVal, clipboard_.texture.posX, 2);
            floatToString(
                clipboard_.texture.posYVal, clipboard_.texture.posY, 2);
            break;
        case SelectableObjectType::Slot:
            clipboard_.slot.posXVal += kModdingPasteNudgeWorld;
            clipboard_.slot.posYVal += kModdingPasteNudgeWorld;
            floatToString(clipboard_.slot.posXVal, clipboard_.slot.posX, 2);
            floatToString(clipboard_.slot.posYVal, clipboard_.slot.posY, 2);
            break;
        case SelectableObjectType::ColliderVertex:
            clipboard_.colliderVertex.xVal += kModdingPasteNudgeWorld;
            clipboard_.colliderVertex.yVal += kModdingPasteNudgeWorld;
            floatToString(
                clipboard_.colliderVertex.xVal, clipboard_.colliderVertex.x, 2);
            floatToString(
                clipboard_.colliderVertex.yVal, clipboard_.colliderVertex.y, 2);
            break;
        case SelectableObjectType::Connector:
            clipboard_.connector.posXVal += kModdingPasteNudgeWorld;
            clipboard_.connector.posYVal += kModdingPasteNudgeWorld;
            floatToString(
                clipboard_.connector.posXVal, clipboard_.connector.posX, 2);
            floatToString(
                clipboard_.connector.posYVal, clipboard_.connector.posY, 2);
            break;
        case SelectableObjectType::TurretExit:
            clipboard_.colliderVertex.xVal += kModdingPasteNudgeWorld;
            clipboard_.colliderVertex.yVal += kModdingPasteNudgeWorld;
            floatToString(
                clipboard_.colliderVertex.xVal, clipboard_.colliderVertex.x, 2);
            floatToString(
                clipboard_.colliderVertex.yVal, clipboard_.colliderVertex.y, 2);
            break;
        default:
            break;
    }
    syncListSelectionToRml();
    return true;
}

bool ModdingTools::deleteSelectedObject()
{
    if (!canEditObjectType(selectedObjectType) || selectedObjectIndex < 0
        || !rmlModel_)
    {
        return false;
    }
    const int i = selectedObjectIndex;
    switch (selectedObjectType)
    {
        case SelectableObjectType::Texture:
            if (i < 0 || i >= static_cast<int>(textures.size()))
            {
                return false;
            }
            textures.erase(textures.begin() + static_cast<size_t>(i));
            if (i < static_cast<int>(textureSizeAppliedForName.size()))
            {
                textureSizeAppliedForName.erase(
                    textureSizeAppliedForName.begin() + static_cast<size_t>(i));
            }
            if (textureRowNameFocusIndex == i)
            {
                textureRowNameFocusIndex = -1;
            }
            else if (textureRowNameFocusIndex > i)
            {
                textureRowNameFocusIndex--;
            }
            fixSelectionAfterErase(SelectableObjectType::Texture, i);
            rmlModel_.DirtyVariable("textures");
            break;
        case SelectableObjectType::Slot:
            if (i < 0 || i >= static_cast<int>(slots.size()))
            {
                return false;
            }
            slots.erase(slots.begin() + static_cast<size_t>(i));
            fixSelectionAfterErase(SelectableObjectType::Slot, i);
            rmlModel_.DirtyVariable("slots");
            break;
        case SelectableObjectType::ColliderVertex:
            if (i < 0 || i >= static_cast<int>(collider.size()))
            {
                return false;
            }
            collider.erase(collider.begin() + static_cast<size_t>(i));
            fixSelectionAfterErase(SelectableObjectType::ColliderVertex, i);
            rmlModel_.DirtyVariable("collider");
            break;
        case SelectableObjectType::Connector:
            if (i < 0 || i >= static_cast<int>(connectors.size()))
            {
                return false;
            }
            connectors.erase(connectors.begin() + static_cast<size_t>(i));
            fixSelectionAfterErase(SelectableObjectType::Connector, i);
            syncStationPartConnectorTextures();
            rmlModel_.DirtyVariable("connectors");
            rmlModel_.DirtyVariable("textures");
            break;
        case SelectableObjectType::TurretExit:
            if (i < 0 || i >= static_cast<int>(turretExits.size()))
            {
                return false;
            }
            turretExits.erase(turretExits.begin() + static_cast<size_t>(i));
            fixSelectionAfterErase(SelectableObjectType::TurretExit, i);
            rmlModel_.DirtyVariable("turretExits");
            break;
        default:
            return false;
    }
    syncListSelectionToRml();
    return true;
}

bool ModdingTools::onEditorKey(ModdingEditorKey editorKey)
{
    if (activeMode != ModdingToolsMode::Hull
        && activeMode != ModdingToolsMode::Module
        && activeMode != ModdingToolsMode::StationPart)
    {
        return false;
    }
    switch (editorKey)
    {
        case ModdingEditorKey::Copy:
            copySelectedToClipboard();
            return clipboard_.valid;
        case ModdingEditorKey::Paste:
            return pasteFromClipboard();
        case ModdingEditorKey::Delete:
            return deleteSelectedObject();
    }
    return false;
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
                && activeMode != ModdingToolsMode::Module
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
        case SelectableObjectType::TurretExit:
            if (activeMode != ModdingToolsMode::Module
                || moduleInfo.moduleTypeVal != gobj::ModuleType::Turret)
            {
                return;
            }
            if (idx < 0 || idx >= static_cast<int>(turretExits.size()))
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
        && activeMode != ModdingToolsMode::Module
        && activeMode != ModdingToolsMode::StationPart)
    {
        return;
    }

    const float colliderPickR = kColliderPickR / worldZoom;
    const float connectorPickR = kConnectorPickR / worldZoom;

    auto clearSelection = [this]()
    {
        selectedObjectType = SelectableObjectType::None;
        selectedObjectIndex = -1;
        syncListSelectionToRml();
    };

    /** Pick priority: structural/edit handles beat decorative layers. Within
     * the same category, lower `z` = foreground; then lower `index` for
     * stability. */
    struct PickCandidate
    {
        SelectableObjectType type = SelectableObjectType::None;
        int index = -1;
        int z = 0;
    };
    auto pickCategoryPriority = [](SelectableObjectType type) -> int
    {
        switch (type)
        {
            case SelectableObjectType::ColliderVertex:
                return 4;
            case SelectableObjectType::Slot:
                return 3;
            case SelectableObjectType::Connector:
                return 2;
            case SelectableObjectType::TurretExit:
                return 2;
            case SelectableObjectType::Texture:
                return 1;
            default:
                return 0;
        }
    };
    auto betterPick = [&](const PickCandidate& a,
                          const PickCandidate& b) -> bool
    {
        const int pa = pickCategoryPriority(a.type);
        const int pb = pickCategoryPriority(b.type);
        if (pa != pb)
        {
            return pa > pb;
        }
        if (a.z != b.z)
        {
            return a.z < b.z;
        }
        return a.index < b.index;
    };
    PickCandidate bestPick{};
    bestPick.index = -1;
    auto consider = [&](SelectableObjectType type, int index, int z)
    {
        PickCandidate c{type, index, z};
        if (bestPick.index < 0 || betterPick(c, bestPick))
        {
            bestPick = c;
        }
    };

    constexpr int kPickImplicitZ = 0;

    if (activeMode == ModdingToolsMode::Hull)
    {
        for (int i = 0; i < static_cast<int>(collider.size()); ++i)
        {
            const glm::vec2 v(collider[static_cast<size_t>(i)].xVal,
                              collider[static_cast<size_t>(i)].yVal);
            if (hitDisc(worldPos, v, colliderPickR))
            {
                consider(
                    SelectableObjectType::ColliderVertex, i, kPickImplicitZ);
            }
        }
        for (int i = 0; i < static_cast<int>(slots.size()); ++i)
        {
            if (hitSlot(worldPos, slots[static_cast<size_t>(i)], renderer))
            {
                consider(SelectableObjectType::Slot,
                         i,
                         slotDrawZ(slots[static_cast<size_t>(i)]));
            }
        }
    }
    else if (activeMode == ModdingToolsMode::Module)
    {
        if (moduleInfo.moduleTypeVal == gobj::ModuleType::Turret)
        {
            for (int i = 0; i < static_cast<int>(turretExits.size()); ++i)
            {
                const glm::vec2 e(turretExits[static_cast<size_t>(i)].xVal,
                                  turretExits[static_cast<size_t>(i)].yVal);
                if (hitDisc(worldPos, e, colliderPickR))
                {
                    consider(
                        SelectableObjectType::TurretExit, i, kPickImplicitZ);
                }
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
                consider(SelectableObjectType::Connector, i, kPickImplicitZ);
            }
        }
        for (int i = 0; i < static_cast<int>(collider.size()); ++i)
        {
            const glm::vec2 v(collider[static_cast<size_t>(i)].xVal,
                              collider[static_cast<size_t>(i)].yVal);
            if (hitDisc(worldPos, v, colliderPickR))
            {
                consider(
                    SelectableObjectType::ColliderVertex, i, kPickImplicitZ);
            }
        }
    }

    for (int i = 0; i < static_cast<int>(textures.size()); ++i)
    {
        if (hitTexture(worldPos, textures[static_cast<size_t>(i)]))
        {
            consider(SelectableObjectType::Texture,
                     i,
                     textures[static_cast<size_t>(i)].zIndexVal);
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
        const TextureInfo& selectedTex =
            textures[static_cast<size_t>(selectedObjectIndex)];
        const glm::vec2 selDraw = moddingTextureDrawSize(selectedTex);
        const float snapDist = std::max(selDraw.x, selDraw.y) * 2.0f
                               + kModdingTexDrawOverlapAddWorld;
        const float snapDistSq = snapDist * snapDist;

        int anchorIdx = -1;
        float bestDistSq = std::numeric_limits<float>::max();
        int bestZ = 0;
        for (int i = 0; i < static_cast<int>(textures.size()); ++i)
        {
            if (i == selectedObjectIndex)
            {
                continue;
            }
            const TextureInfo& cand = textures[static_cast<size_t>(i)];
            const float d2 = distSqPointToModdingTexture(worldPos, cand);
            if (d2 > snapDistSq)
            {
                continue;
            }
            const int zi = cand.zIndexVal;
            const bool betterDist = d2 < bestDistSq - 1.0e-6f;
            const bool sameDistPreferBack =
                std::fabs(d2 - bestDistSq) <= 1.0e-6f
                && (anchorIdx < 0 || zi < bestZ
                    || (zi == bestZ && i < anchorIdx));
            if (anchorIdx < 0 || betterDist || sameDistPreferBack)
            {
                anchorIdx = i;
                bestDistSq = d2;
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
    (void)renderer;
    (void)shiftAlignTextures;
    lmbDragPressWorld = worldPos;
    lmbDragThresholdCfg = dragThresholdWorld;
    lmbPastDragDeadzone = false;
    suppressDragAfterClick = false;
    lmbWorldPressTracked = true;
    lmbHadSelectionOnPress = selectedObjectType != SelectableObjectType::None;
    dragSelectedObject = lmbHadSelectionOnPress;
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
        case SelectableObjectType::TurretExit:
            if (selectedObjectIndex >= static_cast<int>(turretExits.size()))
            {
                return;
            }
            turretExits[static_cast<size_t>(selectedObjectIndex)].xVal =
                worldPos.x;
            turretExits[static_cast<size_t>(selectedObjectIndex)].yVal =
                worldPos.y;
            floatToString(
                turretExits[static_cast<size_t>(selectedObjectIndex)].xVal,
                turretExits[static_cast<size_t>(selectedObjectIndex)].x,
                2);
            floatToString(
                turretExits[static_cast<size_t>(selectedObjectIndex)].yVal,
                turretExits[static_cast<size_t>(selectedObjectIndex)].y,
                2);
            rmlModel_.DirtyVariable("turretExits");
            break;
        case SelectableObjectType::None:
        default:
            break;
    }
}

void ModdingTools::onLeftMouseUp(const glm::vec2& worldPos,
                                 float worldZoom,
                                 gfx::RenderEngine* renderer,
                                 bool shiftAlignTextures)
{
    if (lmbWorldPressTracked
        && (!lmbHadSelectionOnPress || !lmbPastDragDeadzone))
    {
        onSingleClick(worldPos, worldZoom, renderer, shiftAlignTextures);
    }
    dragSelectedObject = false;
    lmbPastDragDeadzone = false;
    lmbHadSelectionOnPress = false;
    lmbWorldPressTracked = false;
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
        && activeMode != ModdingToolsMode::Module
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
    (void)worldZoom;
    if (!rmbRotateGesture)
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

    const glm::vec2 fromPrev = rmbRotatePrevWorld - pivot;
    const glm::vec2 toCur = worldPos - pivot;
    if (glm::dot(fromPrev, fromPrev) < 1e-8f || glm::dot(toCur, toCur) < 1e-8f)
    {
        rmbRotatePrevWorld = worldPos;
        return;
    }
    const float anglePrev = std::atan2(fromPrev.y, fromPrev.x);
    const float angleCur = std::atan2(toCur.y, toCur.x);
    float deltaRad = angleCur - anglePrev;
    constexpr float kPi = 3.14159265358979323846f;
    while (deltaRad > kPi)
    {
        deltaRad -= 2.0f * kPi;
    }
    while (deltaRad < -kPi)
    {
        deltaRad += 2.0f * kPi;
    }
    *rotDeg += smath::radToDeg(deltaRad);
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
        "onAutoGenerateCollider", &ModdingTools::onAutoGenerateCollider, this);
    moddingToolsConstructor.BindEventCallback(
        "onAddConnector", &ModdingTools::onAddConnector, this);
    moddingToolsConstructor.BindEventCallback(
        "onClearConnectors", &ModdingTools::onClearConnectors, this);
    moddingToolsConstructor.BindEventCallback(
        "onRemoveConnector", &ModdingTools::onRemoveConnector, this);
    moddingToolsConstructor.BindEventCallback(
        "onAddTurretExit", &ModdingTools::onAddTurretExit, this);
    moddingToolsConstructor.BindEventCallback(
        "onClearTurretExits", &ModdingTools::onClearTurretExits, this);
    moddingToolsConstructor.BindEventCallback(
        "onRemoveTurretExit", &ModdingTools::onRemoveTurretExit, this);
    moddingToolsConstructor.BindEventCallback(
        "onTextureNameFocus", &ModdingTools::onTextureNameFocus, this);
    moddingToolsConstructor.BindEventCallback(
        "onGlobalNewTexturePickerFocus",
        &ModdingTools::onGlobalNewTexturePickerFocus,
        this);
    moddingToolsConstructor.BindEventCallback(
        "onTextureRowNonNameFocus",
        &ModdingTools::onTextureRowNonNameFocus,
        this);
    moddingToolsConstructor.BindEventCallback(
        "onPickTextureName", &ModdingTools::onPickTextureName, this);
    moddingToolsConstructor.BindEventCallback(
        "onPickNewTextureNameFromPicker",
        &ModdingTools::onPickNewTextureNameFromPicker,
        this);
    moddingToolsConstructor.BindEventCallback(
        "onModdingSelectListRow", &ModdingTools::onModdingSelectListRow, this);
    if (auto storageVolumesHandle =
            moddingToolsConstructor.RegisterStruct<StorageVolumesInfo>())
    {
        storageVolumesHandle.RegisterMember("containerS",
                                            &StorageVolumesInfo::containerS);
        storageVolumesHandle.RegisterMember("containerL",
                                            &StorageVolumesInfo::containerL);
        storageVolumesHandle.RegisterMember("tank", &StorageVolumesInfo::tank);
        storageVolumesHandle.RegisterMember("bulk", &StorageVolumesInfo::bulk);
    }
    if (auto hullHandle = moddingToolsConstructor.RegisterStruct<GeneralInfo>())
    {
        hullHandle.RegisterMember("name", &GeneralInfo::name);
        hullHandle.RegisterMember("hp", &GeneralInfo::hp);
        hullHandle.RegisterMember("width", &GeneralInfo::width);
        hullHandle.RegisterMember("length", &GeneralInfo::length);
        hullHandle.RegisterMember("mass", &GeneralInfo::mass);
        hullHandle.RegisterMember("inertia", &GeneralInfo::inertia);
        hullHandle.RegisterMember("internalGyroTorque",
                                  &GeneralInfo::internalGyroTorque);
        hullHandle.RegisterMember("shipClass", &GeneralInfo::shipClass);
        hullHandle.RegisterMember("storageVolumes",
                                  &GeneralInfo::storageVolumes);
        hullHandle.RegisterMember("colliderRestitution",
                                  &GeneralInfo::colliderRestitutionVal);
    }
    moddingToolsConstructor.Bind("hull", &genInfo);
    if (auto stationPartHandle =
            moddingToolsConstructor.RegisterStruct<StationPartInfo>())
    {
        stationPartHandle.RegisterMember("partType",
                                         &StationPartInfo::partType);
        stationPartHandle.RegisterMember("storageVolumes",
                                         &StationPartInfo::storageVolumes);
    }
    moddingToolsConstructor.Bind("stationPart", &stationPartInfo);
    if (auto moduleHandle =
            moddingToolsConstructor.RegisterStruct<ModuleInfo>())
    {
        moduleHandle.RegisterMember("moduleType", &ModuleInfo::moduleType);
        moduleHandle.RegisterMember("slotType", &ModuleInfo::slotType);
        moduleHandle.RegisterMember("description", &ModuleInfo::description);
        moduleHandle.RegisterMember("mass", &ModuleInfo::mass);
        moduleHandle.RegisterMember("maxThrust", &ModuleInfo::maxThrust);
        moduleHandle.RegisterMember("storageVolumes",
                                    &ModuleInfo::storageVolumes);
        moduleHandle.RegisterMember("hangarMaxShipClass",
                                    &ModuleInfo::hangarMaxShipClass);
        moduleHandle.RegisterMember("hangarSpace", &ModuleInfo::hangarSpace);
        moduleHandle.RegisterMember("turretType", &ModuleInfo::turretType);
        moduleHandle.RegisterMember("turretDamageType",
                                    &ModuleInfo::turretDamageType);
        moduleHandle.RegisterMember("turretNumBarrels",
                                    &ModuleInfo::turretNumBarrels);
        moduleHandle.RegisterMember("turretProjDmg",
                                    &ModuleInfo::turretProjDmg);
        moduleHandle.RegisterMember("turretExitSpeed",
                                    &ModuleInfo::turretExitSpeed);
        moduleHandle.RegisterMember("turretLifetime",
                                    &ModuleInfo::turretLifetime);
        moduleHandle.RegisterMember("turretReloadTime",
                                    &ModuleInfo::turretReloadTime);
        moduleHandle.RegisterMember("turretDps", &ModuleInfo::turretDps);
        moduleHandle.RegisterMember("turretBeamWidth",
                                    &ModuleInfo::turretBeamWidth);
        moduleHandle.RegisterMember("turretBeamLength",
                                    &ModuleInfo::turretBeamLength);
        moduleHandle.RegisterMember("turretArcAngle",
                                    &ModuleInfo::turretArcAngle);
        moduleHandle.RegisterMember("turretArcLength",
                                    &ModuleInfo::turretArcLength);
    }
    moddingToolsConstructor.Bind("module", &moduleInfo);
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
        textureHandle.RegisterMember("tileCntX", &TextureInfo::tileCntX);
        textureHandle.RegisterMember("tileCntY", &TextureInfo::tileCntY);
        textureHandle.RegisterMember("tileOffX", &TextureInfo::tileOffX);
        textureHandle.RegisterMember("tileOffY", &TextureInfo::tileOffY);
        textureHandle.RegisterMember("tileModifiers",
                                     &TextureInfo::tileModifiers);
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
        slotHandle.RegisterMember("minAngle", &SlotInfo::minAngle);
        slotHandle.RegisterMember("maxAngle", &SlotInfo::maxAngle);
        slotHandle.RegisterMember("hasAngleLimits", &SlotInfo::hasAngleLimits);
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
    if (auto turretExitHandle =
            moddingToolsConstructor.RegisterStruct<TurretExitInfo>())
    {
        turretExitHandle.RegisterMember("x", &TurretExitInfo::x);
        turretExitHandle.RegisterMember("y", &TurretExitInfo::y);
    }
    moddingToolsConstructor.RegisterArray<std::vector<TurretExitInfo>>();
    moddingToolsConstructor.Bind("turretExits", &turretExits);

    moddingToolsConstructor.Bind("openFilepath", &openFilepath);
    moddingToolsConstructor.Bind("extendTextures", &extendTextures);
    moddingToolsConstructor.Bind("extendSlots", &extendSlots);
    moddingToolsConstructor.Bind("extendCollider", &extendCollider);
    moddingToolsConstructor.Bind("extendConnectors", &extendConnectors);

    rmlModel_ = moddingToolsConstructor.GetModelHandle();
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
    turretExits.clear();
    genInfo.colliderRestitutionVal = 0.1f;
    selectedObjectType = SelectableObjectType::None;
    selectedObjectIndex = -1;
    textureRowNameFocusIndex = -1;
    syncModeToRml();
    updateHullDerivedFromCollider();
    handle.DirtyVariable("openFilepath");
    handle.DirtyVariable("hull");
    handle.DirtyVariable("stationPart");
    handle.DirtyVariable("textures");
    handle.DirtyVariable("slots");
    handle.DirtyVariable("collider");
    handle.DirtyVariable("connectors");
    handle.DirtyVariable("turretExits");
    syncListSelectionToRml();
    LG_D("Modding tools: new hull");
}

void ModdingTools::onModdingNewModule(Rml::DataModelHandle handle,
                                      Rml::Event& event,
                                      const Rml::VariantList& args)
{
    (void)event;
    (void)args;
    activeMode = ModdingToolsMode::Module;
    openFilepath = "";
    genInfo = GeneralInfo{};
    moduleInfo = ModuleInfo{};
    stationPartInfo = StationPartInfo{};
    textures.clear();
    textureSizeAppliedForName.clear();
    resetNewTexturePickerState();
    slots.clear();
    collider.clear();
    connectors.clear();
    turretExits.clear();
    turretExits.clear();
    selectedObjectType = SelectableObjectType::None;
    selectedObjectIndex = -1;
    textureRowNameFocusIndex = -1;
    syncModeToRml();
    handle.DirtyVariable("openFilepath");
    handle.DirtyVariable("hull");
    handle.DirtyVariable("module");
    handle.DirtyVariable("textures");
    handle.DirtyVariable("turretExits");
    syncListSelectionToRml();
    LG_D("Modding tools: new module");
}

void ModdingTools::onModdingNewStationPart(Rml::DataModelHandle handle,
                                           Rml::Event& event,
                                           const Rml::VariantList& args)
{
    activeMode = ModdingToolsMode::StationPart;
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
    handle.DirtyVariable("turretExits");
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
        case ModdingToolsMode::Module:
        {
            if (textures.empty())
            {
                LG_W("Modding tools: no textures to save");
                return;
            }
            if (!saveModuleDataToPath(path))
            {
                LG_W("Modding tools: failed to write {}", path);
                return;
            }
            openFilepath = std::move(path);
            handle.DirtyVariable("openFilepath");
            LG_D("Modding tools: saved module to {}", openFilepath);
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
        case ModdingToolsMode::Module:
            if (!loadModuleDataFromPath(path))
            {
                LG_W("Modding tools: failed to load module from {}", path);
                return;
            }
            LG_D("Modding tools: loaded module from {}", openFilepath);
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
    handle.DirtyVariable("module");
    handle.DirtyVariable("textures");
    handle.DirtyVariable("slots");
    handle.DirtyVariable("collider");
    handle.DirtyVariable("connectors");
    handle.DirtyVariable("turretExits");
    handle.DirtyVariable("stationPart");
    handle.DirtyVariable("mode");
    textureSizeAppliedForName.resize(textures.size());
    for (size_t i = 0; i < textures.size(); ++i)
    {
        textureSizeAppliedForName[i] = makeTextureSizeSyncKey(textures[i]);
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
    refreshPerRowTextureNameSuggestions();
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
        && activeMode != ModdingToolsMode::Module
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

void ModdingTools::applyTextureNameToRow(int rowIndex, const string& pickedName)
{
    if (rowIndex < 0 || rowIndex >= static_cast<int>(textures.size())
        || pickedName.empty())
    {
        return;
    }
    TextureInfo& tex = textures[static_cast<size_t>(rowIndex)];
    if (tex.name == pickedName)
    {
        return;
    }
    tex.name = pickedName;
    if (rowIndex < static_cast<int>(textureSizeAppliedForName.size()))
    {
        textureSizeAppliedForName[static_cast<size_t>(rowIndex)].clear();
    }
    syncTextureTileStrings(tex);
    if (rmlModel_)
    {
        rmlModel_.DirtyVariable("textures");
    }
}

void ModdingTools::onPickTextureName(Rml::DataModelHandle handle,
                                     Rml::Event& event,
                                     const Rml::VariantList& args)
{
    (void)event;
    (void)handle;
    if (args.size() != 2)
    {
        return;
    }
    const int rowFromUi = args[0].Get<int>(-1);
    const string picked = args[1].Get<string>("");
    if (picked.empty())
    {
        return;
    }
    int row = rowFromUi;
    if (row < 0 || row >= static_cast<int>(textures.size()))
    {
        row = textureRowNameFocusIndex;
    }
    if (row < 0 || row >= static_cast<int>(textures.size()))
    {
        return;
    }
    applyTextureNameToRow(row, picked);
    textureRowNameFocusIndex = row;
    selectedObjectType = SelectableObjectType::Texture;
    selectedObjectIndex = row;
    refreshPerRowTextureNameSuggestions();
    syncListSelectionToRml();
}

void ModdingTools::onAddSlot(Rml::DataModelHandle handle,
                             Rml::Event& event,
                             const Rml::VariantList& args)
{
    (void)event;
    (void)args;
    slots.push_back(SlotInfo{});
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
    updateHullDerivedFromCollider();
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
    updateHullDerivedFromCollider();
    rmlModel_.DirtyVariable("collider");
    syncListSelectionToRml();
}

void ModdingTools::generateColliderFromVisibleTextures(
    gfx::RenderEngine& renderer)
{
    if (activeMode != ModdingToolsMode::Hull
        && activeMode != ModdingToolsMode::Module
        && activeMode != ModdingToolsMode::StationPart)
    {
        return;
    }

    parseEditorNumericFields();

    std::vector<glm::vec2> samples;
    samples.reserve(4096);
    for (const TextureInfo& tex : textures)
    {
        if (tex.name.empty())
        {
            continue;
        }
        std::string path;
        if (!renderer.getTextureFilePath(tex.name, path))
        {
            continue;
        }
        const std::optional<RgbaImage> image = loadRgbaImageFromPath(path);
        if (!image)
        {
            LG_W("Modding tools: could not load texture image for collider: {}",
                 tex.name);
            continue;
        }
        appendOpaqueTextureSamples(tex, *image, samples);
    }

    collider.clear();
    selectedObjectType = SelectableObjectType::None;
    selectedObjectIndex = -1;

    if (samples.size() < 3)
    {
        LG_W(
            "Modding tools: not enough opaque texture samples for collider "
            "(need at least 3)");
        if (rmlModel_)
        {
            rmlModel_.DirtyVariable("collider");
        }
        syncListSelectionToRml();
        return;
    }

    const std::vector<glm::vec2> hull = convexHull(std::move(samples));
    collider.reserve(hull.size());
    for (const glm::vec2& p : hull)
    {
        ColliderVertex v;
        v.xVal = p.x;
        v.yVal = p.y;
        floatToString(v.xVal, v.x, 2);
        floatToString(v.yVal, v.y, 2);
        collider.push_back(std::move(v));
    }

    updateHullDerivedFromCollider();
    if (rmlModel_)
    {
        rmlModel_.DirtyVariable("collider");
    }
    syncListSelectionToRml();
    LG_I("Modding tools: generated collider with {} vertices", collider.size());
}

void ModdingTools::onAutoGenerateCollider(Rml::DataModelHandle handle,
                                          Rml::Event& event,
                                          const Rml::VariantList& args)
{
    (void)handle;
    (void)event;
    (void)args;
    if (drawRenderer_ == nullptr)
    {
        LG_W("Modding tools: auto collider requires an active render view");
        return;
    }
    generateColliderFromVisibleTextures(*drawRenderer_);
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
        updateHullDerivedFromCollider();
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

void ModdingTools::syncTurretNumBarrelsFromExits()
{
    applyTurretNumBarrelsFromExits(moduleInfo, turretExits);
    if (rmlModel_)
    {
        rmlModel_.DirtyVariable("module");
    }
}

void ModdingTools::onAddTurretExit(Rml::DataModelHandle handle,
                                   Rml::Event& event,
                                   const Rml::VariantList& args)
{
    (void)event;
    (void)args;
    turretExits.push_back(TurretExitInfo{});
    syncTurretNumBarrelsFromExits();
    handle.DirtyVariable("turretExits");
}

void ModdingTools::onClearTurretExits(Rml::DataModelHandle handle,
                                      Rml::Event& event,
                                      const Rml::VariantList& args)
{
    (void)event;
    (void)args;
    turretExits.clear();
    syncTurretNumBarrelsFromExits();
    handle.DirtyVariable("turretExits");
}

void ModdingTools::onRemoveTurretExit(Rml::DataModelHandle handle,
                                      Rml::Event& event,
                                      const Rml::VariantList& args)
{
    (void)event;
    if (args.size() != 1)
    {
        return;
    }
    const int i = args[0].Get<int>(-1);
    if (i >= 0 && i < static_cast<int>(turretExits.size()))
    {
        turretExits.erase(turretExits.begin() + static_cast<size_t>(i));
        syncTurretNumBarrelsFromExits();
        handle.DirtyVariable("turretExits");
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
        t.zIndexVal = 0;
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

void ModdingTools::updateHullDerivedFromCollider()
{
    if (activeMode != ModdingToolsMode::Hull)
    {
        return;
    }
    vector<vec2> verts;
    verts.reserve(collider.size());
    for (const ColliderVertex& v : collider)
    {
        verts.push_back(vec2(v.xVal, v.yVal));
    }
    const std::optional<vec2> ext = gobj::colliderLocalExtents(verts);
    if (ext.has_value())
    {
        genInfo.widthVal = ext->x;
        genInfo.lengthVal = ext->y;
    }
    else
    {
        genInfo.widthVal = 0.0f;
        genInfo.lengthVal = 0.0f;
    }
    floatToString(genInfo.widthVal, genInfo.width, 2);
    floatToString(genInfo.lengthVal, genInfo.length, 2);
    genInfo.shipClassVal = gobj::inferShipClassFromColliderVertices(verts);
    genInfo.shipClass = string(magic_enum::enum_name(genInfo.shipClassVal));
    genInfo.inertiaVal = gobj::approximateHullInertia(
        genInfo.massVal, genInfo.widthVal, genInfo.lengthVal);
    formatModdingInertiaKString(genInfo.inertiaVal, genInfo.inertia);
    if (rmlModel_)
    {
        rmlModel_.DirtyVariable("hull");
    }
}

void ModdingTools::parseEditorNumericFields()
{
    parseGeneralInfoNumericFields(genInfo, hpVal);

    for (auto& texture : textures)
    {
        tryParseFloat(texture.posX, texture.posXVal);
        tryParseFloat(texture.posY, texture.posYVal);
        tryParseFloat(texture.sizeX, texture.sizeXVal);
        tryParseFloat(texture.sizeY, texture.sizeYVal);
        tryParseFloat(texture.rot, texture.rotVal);
        tryParseInt(texture.zIndex, texture.zIndexVal);
        tryParseFloat(texture.tileCntX, texture.tileCntXVal);
        tryParseFloat(texture.tileCntY, texture.tileCntYVal);
        tryParseFloat(texture.tileOffX, texture.tileOffXVal);
        tryParseFloat(texture.tileOffY, texture.tileOffYVal);
        floatToString(texture.posXVal, texture.posX, 2);
        floatToString(texture.posYVal, texture.posY, 2);
        floatToString(texture.sizeXVal, texture.sizeX, 2);
        floatToString(texture.sizeYVal, texture.sizeY, 2);
        floatToString(texture.rotVal, texture.rot, 2);
        intToString(texture.zIndexVal, texture.zIndex);
        syncTextureTileStrings(texture);
        rmlModel_.DirtyVariable("texture");
    }
    for (auto& slot : slots)
    {
        tryParseFloat(slot.posX, slot.posXVal);
        tryParseFloat(slot.posY, slot.posYVal);
        tryParseFloat(slot.rot, slot.rotVal);
        tryParseFloat(slot.minAngle, slot.minAngleVal);
        tryParseFloat(slot.maxAngle, slot.maxAngleVal);
        floatToString(slot.posXVal, slot.posX, 2);
        floatToString(slot.posYVal, slot.posY, 2);
        floatToString(slot.rotVal, slot.rot, 2);
        floatToString(slot.minAngleVal, slot.minAngle, 2);
        floatToString(slot.maxAngleVal, slot.maxAngle, 2);
        auto slotType =
            magic_enum::enum_cast<gobj::ModuleSlotType>(slot.slotType);
        if (slotType.has_value())
        {
            slot.slotTypeVal = slotType.value();
        }
        slot.hasAngleLimits =
            gobj::moduleSlotTypeHasAngleLimits(slot.slotTypeVal);
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
    parseStorageVolumesInfoStrings(stationPartInfo.storageVolumes);
    parseModuleInfoNumericFields(moduleInfo);
    if (moduleInfo.moduleTypeVal == gobj::ModuleType::Turret)
    {
        syncTurretNumBarrelsFromExits();
    }
    if (rmlModel_)
    {
        rmlModel_.DirtyVariable("module");
        rmlModel_.DirtyVariable("stationPart");
    }

    for (auto& connector : connectors)
    {
        tryParseFloat(connector.posX, connector.posXVal);
        tryParseFloat(connector.posY, connector.posYVal);
        tryParseFloat(connector.rot, connector.rotDegVal);
        floatToString(connector.posXVal, connector.posX, 2);
        floatToString(connector.posYVal, connector.posY, 2);
        floatToString(connector.rotDegVal, connector.rot, 2);
    }
    for (auto& turretExit : turretExits)
    {
        tryParseFloat(turretExit.x, turretExit.xVal);
        tryParseFloat(turretExit.y, turretExit.yVal);
        floatToString(turretExit.xVal, turretExit.x, 2);
        floatToString(turretExit.yVal, turretExit.y, 2);
    }
    rmlModel_.DirtyVariable("connectors");
    rmlModel_.DirtyVariable("turretExits");
    rmlModel_.DirtyVariable("stationPart");
    rmlModel_.DirtyVariable("hull");
    if (activeMode == ModdingToolsMode::Hull)
    {
        updateHullDerivedFromCollider();
    }
    if (activeMode == ModdingToolsMode::StationPart)
    {
        syncStationPartConnectorTextures();
        rmlModel_.DirtyVariable("textures");
    }
}

void ModdingTools::refreshPerRowTextureNameSuggestions()
{
    if (activeMode != ModdingToolsMode::Hull
        && activeMode != ModdingToolsMode::Module
        && activeMode != ModdingToolsMode::StationPart)
    {
        return;
    }
    bool anyChange = false;
    for (size_t i = 0; i < textures.size(); ++i)
    {
        if (static_cast<int>(i) != textureRowNameFocusIndex
            && !textures[i].nameSuggestions.empty())
        {
            textures[i].nameSuggestions.clear();
            anyChange = true;
        }
    }
    if (textureRowNameFocusIndex < 0
        || textureRowNameFocusIndex >= static_cast<int>(textures.size()))
    {
        if (anyChange && rmlModel_)
        {
            rmlModel_.DirtyVariable("textures");
        }
        return;
    }

    TextureInfo& tex = textures[static_cast<size_t>(textureRowNameFocusIndex)];
    if (!textureNameUsesBoundsBleed(tex.name))
    {
        if (!tex.nameSuggestions.empty())
        {
            tex.nameSuggestions.clear();
            anyChange = true;
        }
    }
    else
    {
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
        && activeMode != ModdingToolsMode::Module
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
        const string syncKey = makeTextureSizeSyncKey(tex);
        if (syncKey == textureSizeAppliedForName[i] || tex.name.empty())
        {
            continue;
        }
        const string previousKey = textureSizeAppliedForName[i];
        textureSizeAppliedForName[i] = syncKey;

        glm::vec2 baseTile{};
        if (tryTextureBaseTileSize(renderer, tex, baseTile))
        {
            setTextureSizeFromBaseTile(tex, baseTile);
            dirty = true;
            continue;
        }

        float oldTileX = 1.0f;
        float oldTileY = 1.0f;
        if (!previousKey.empty()
            && parseTextureSizeSyncKeyTileCounts(
                previousKey, oldTileX, oldTileY))
        {
            baseTile.x = tex.sizeXVal / oldTileX;
            baseTile.y = tex.sizeYVal / oldTileY;
            setTextureSizeFromBaseTile(tex, baseTile);
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
    texture.zIndexVal = 0;
    intToString(texture.zIndexVal, texture.zIndex);
    syncTextureTileStrings(texture);
    if ((activeMode == ModdingToolsMode::Hull
         || activeMode == ModdingToolsMode::Module
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
    drawRenderer_ = &renderer;
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
        || activeMode == ModdingToolsMode::Module
        || activeMode == ModdingToolsMode::StationPart)
    {
        constexpr float kWorldAxesHalfExtent = 10000.0f;
        constexpr float kWorldAxesZ = 100.0f;
        const float zoom = renderer.getWorldZoom();
        const float axisThickness = zoom > 1e-6f ? (2.0f / zoom) : 2.0f;
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
            drawTextures(renderer, gfx::RenderEngine::zIdxShipHull);
            drawSlots(renderer);
            drawColliders(renderer);
            break;
        case ModdingToolsMode::Module:
        {
            const int8_t z = gfx::RenderEngine::zIdxShipHull
                             + gobj::ModuleSlotZOffset[static_cast<size_t>(
                                 moduleInfo.slotTypeVal)];
            drawTextures(renderer, z);
            if (moduleInfo.moduleTypeVal == gobj::ModuleType::Turret)
            {
                drawTurretExits(renderer);
            }
            break;
        }
        case ModdingToolsMode::StationPart:
            drawTextures(renderer, gfx::RenderEngine::zIdxStation);
            drawColliders(renderer);
            drawConnectors(renderer);
            break;
        default:
            // No drawing for other modes
            break;
    }
}

void ModdingTools::drawTextures(gfx::RenderEngine& renderer, int8_t zParent)
{
    for (size_t i = 0; i < textures.size(); ++i)
    {
        auto& texture = textures[i];
        const gfx::TextureHandle texHandle =
            renderer.getTextureHandle(texture.name);
        const bool sel = selectedObjectType == SelectableObjectType::Texture
                         && selectedObjectIndex == static_cast<int>(i);
        const glm::vec2 drawSize = moddingTextureDrawSize(texture);
        // Match Model::drawTextures / drawTexRect: body rot 0 → pass
        // -texture.rot
        renderer.queueTexRect(
            glm::vec2(texture.posXVal, texture.posYVal),
            drawSize,
            texHandle,
            -smath::degToRad(texture.rotVal),
            zParent + texture.zIndexVal,
            tintIfSelected(0xffffffff, sel),
            0,
            glm::vec2(texture.tileOffXVal, texture.tileOffYVal),
            glm::vec2(texture.tileCntXVal, texture.tileCntYVal));
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
    const glm::vec2 dotRadius(kColliderDotSize / zoom, kColliderDotSize / zoom);
    const float lineWidth = 0.66f * kColliderDotSize / zoom;

    for (size_t i = 0; i < collider.size(); ++i)
    {
        const auto& vertex = collider[i];
        const bool sel =
            selectedObjectType == SelectableObjectType::ColliderVertex
            && selectedObjectIndex == static_cast<int>(i);
        renderer.drawEllipse(glm::vec2(vertex.xVal, vertex.yVal),
                             dotRadius,
                             tintIfSelected(kColliderColor, sel),
                             0.0f,
                             0.0f,
                             0.0f,
                             0);
    }
    if (collider.size() > 1)
    {
        ColliderVertex& first = collider.back();
        for (size_t i = 0; i < collider.size(); ++i)
        {
            ColliderVertex& current = collider[i];
            const bool selSeg =
                selectedObjectType == SelectableObjectType::ColliderVertex
                && (selectedObjectIndex == static_cast<int>(i)
                    || selectedObjectIndex == static_cast<int>(i - 1));
            renderer.drawLine(glm::vec2(first.xVal, first.yVal),
                              glm::vec2(current.xVal, current.yVal),
                              tintIfSelected(kColliderColor,
                                             selSeg),  // A nice sky blue color
                              lineWidth,
                              0.0f,
                              0);

            first = current;
        }
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

void ModdingTools::drawTurretExits(gfx::RenderEngine& renderer)
{
    constexpr uint32_t kTurretExitColor = 0xff40c0ffu;
    constexpr float kTurretExitLineLength = 1.0f;
    const float zoom = renderer.getWorldZoom();
    const float lineW = 1.0f / std::max(zoom, 1e-3f);
    for (const auto& exit : turretExits)
    {
        const glm::vec2 p0(exit.xVal, exit.yVal);
        const glm::vec2 p1(exit.xVal, exit.yVal + kTurretExitLineLength);
        renderer.drawLine(p0, p1, kTurretExitColor, lineW, 0.0f, 0);
    }
}

void ModdingTools::drawRoofSlot(gfx::RenderEngine& renderer,
                                const SlotInfo& slot,
                                bool selected)
{
    const float zoom = renderer.getWorldZoom();
    // drawExampleSlotTexture(renderer,
    //                        slot,
    //                        exampleSlotTextureName(slot.slotTypeVal),
    //                        0xffffff00,
    //                        selected);
    const vec2 pos = glm::vec2(slot.posXVal, slot.posYVal);
    const vec2 vert = vec2(0.0f, 2.0f);
    const vec2 horz = vec2(2.0f, 0.0f);
    renderer.drawLine(pos - vert, pos + vert, 0xffffff00, 1.0f / zoom, 0.0f, 0);
    renderer.drawLine(pos - horz, pos + horz, 0xffffff00, 1.0f / zoom, 0.0f, 0);
}

void ModdingTools::drawThrusterMainSlot(gfx::RenderEngine& renderer,
                                        const SlotInfo& slot,
                                        bool selected)
{
    drawExampleSlotTexture(renderer,
                           slot,
                           exampleSlotTextureName(slot.slotTypeVal),
                           0xffffff00,
                           selected);
}

void ModdingTools::drawThrusterManeuverSlot(gfx::RenderEngine& renderer,
                                            const SlotInfo& slot,
                                            bool selected)
{
    drawExampleSlotTexture(renderer,
                           slot,
                           exampleSlotTextureName(slot.slotTypeVal),
                           0xffffff00,
                           selected);
}

void ModdingTools::drawInternalSlot(gfx::RenderEngine& renderer,
                                    const SlotInfo& slot,
                                    bool selected)
{
    drawExampleSlotTexture(renderer,
                           slot,
                           exampleSlotTextureName(slot.slotTypeVal),
                           0xff44ff44,
                           selected);
}

void ModdingTools::drawBaySlot(gfx::RenderEngine& renderer,
                               const SlotInfo& slot,
                               bool selected)
{
    drawExampleSlotTexture(renderer,
                           slot,
                           exampleSlotTextureName(slot.slotTypeVal),
                           0xffffff00,
                           selected);
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
    turretExits.clear();
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
        if (hullNode["mass"])
        {
            genInfo.massVal = hullNode["mass"].as<float>();
            formatModdingMassTonsString(genInfo.massVal, genInfo.mass);
        }
        if (hullNode["internal-gyro-torque"])
        {
            genInfo.internalGyroTorqueVal =
                hullNode["internal-gyro-torque"].as<float>();
            formatModdingTorqueKNmString(genInfo.internalGyroTorqueVal,
                                         genInfo.internalGyroTorque);
        }
        loadModuleStorageVolumesFromYaml(hullNode, genInfo.storageVolumes);

        string texKey;
        if (hullNode["textures"])
        {
            texKey = hullNode["textures"].as<string>();
        }
        TextureYamlLoadOptions texOpts;
        texOpts.yamlTextureBoundsBleed = yamlTextureBoundsBleed;
        loadTexturesFromRootBundle(root, texKey, textures, texOpts);
        if (!texKey.empty() && textures.empty())
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

                if (gobj::moduleSlotTypeHasAngleLimits(s.slotTypeVal))
                {
                    float minAngleDeg = -180.0f;
                    float maxAngleDeg = 180.0f;
                    if (sn["min-angle"])
                    {
                        minAngleDeg = sn["min-angle"].as<float>();
                    }
                    if (sn["max-angle"])
                    {
                        maxAngleDeg = sn["max-angle"].as<float>();
                    }
                    s.minAngleVal = minAngleDeg;
                    s.maxAngleVal = maxAngleDeg;
                    floatToString(s.minAngleVal, s.minAngle, 2);
                    floatToString(s.maxAngleVal, s.maxAngle, 2);
                }
                s.hasAngleLimits =
                    gobj::moduleSlotTypeHasAngleLimits(s.slotTypeVal);

                slots.push_back(std::move(s));
            }
        }

        const YAML::Node colliderRoot = root["collider"];
        if (colliderRoot && colliderRoot[hullMapKey]
            && colliderRoot[hullMapKey].IsMap())
        {
            loadColliderVerticesFromEntry(colliderRoot[hullMapKey],
                                          collider,
                                          genInfo.colliderRestitutionVal);
        }
        updateHullDerivedFromCollider();
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
    const YAML::Node moduleMap = root["module"];
    if (moduleMap && moduleMap.IsMap() && moduleMap.size() > 0)
    {
        return ModdingToolsMode::Module;
    }
    const YAML::Node spMap = root["station-part"];
    if (spMap && spMap.IsMap() && spMap.size() > 0)
    {
        return ModdingToolsMode::StationPart;
    }
    return ModdingToolsMode::None;
}

bool ModdingTools::loadModuleDataFromPath(const string& path)
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

    const YAML::Node moduleMap = root["module"];
    if (!moduleMap || !moduleMap.IsMap() || moduleMap.size() == 0)
    {
        LG_W("Modding tools: {} has no module map", path);
        return false;
    }

    const auto modIt = moduleMap.begin();
    const YAML::Node modNode = modIt->second;
    if (!modNode || !modNode.IsMap())
    {
        LG_W("Modding tools: invalid module entry in {}", path);
        return false;
    }

    activeMode = ModdingToolsMode::Module;
    syncModeToRml();
    textures.clear();
    slots.clear();
    collider.clear();
    connectors.clear();
    moduleInfo = ModuleInfo{};
    stationPartInfo = StationPartInfo{};
    genInfo = GeneralInfo{};

    const string moduleMapKey = modIt->first.as<string>();

    try
    {
        const bool yamlTextureBoundsBleed =
            root[kYamlTextureBoundsBleedKey].IsDefined()
            && root[kYamlTextureBoundsBleedKey].as<bool>();

        if (modNode["name"])
        {
            genInfo.name = modNode["name"].as<string>();
        }
        if (genInfo.name.empty())
        {
            genInfo.name = moduleMapKey;
        }

        if (modNode["description"])
        {
            moduleInfo.description = modNode["description"].as<string>();
        }

        string slotTypeStr = "ThrusterMainS_Common";
        if (modNode["slot-type"])
        {
            slotTypeStr = modNode["slot-type"].as<string>();
        }
        moduleInfo.slotType = slotTypeStr;
        const auto st =
            magic_enum::enum_cast<gobj::ModuleSlotType>(moduleInfo.slotType);
        if (st.has_value())
        {
            moduleInfo.slotTypeVal = st.value();
        }
        else
        {
            moduleInfo.slotType = "ThrusterMainS_Common";
            moduleInfo.slotTypeVal = gobj::ModuleSlotType::ThrusterMainS_Common;
        }

        string typeStr = "MainThruster";
        if (modNode["type"])
        {
            typeStr = modNode["type"].as<string>();
        }
        moduleInfo.moduleType = typeStr;
        const auto mt =
            magic_enum::enum_cast<gobj::ModuleType>(moduleInfo.moduleType);
        if (mt.has_value())
        {
            moduleInfo.moduleTypeVal = mt.value();
        }
        else
        {
            moduleInfo.moduleType = "MainThruster";
            moduleInfo.moduleTypeVal = gobj::ModuleType::MainThruster;
        }

        if (modNode["mass"])
        {
            moduleInfo.massVal = modNode["mass"].as<float>();
            formatModdingMassTonsString(moduleInfo.massVal, moduleInfo.mass);
        }

        moduleInfo.maxThrustVal = 100000.0f;
        formatModdingThrustMNString(moduleInfo.maxThrustVal,
                                    moduleInfo.maxThrust);
        moduleInfo.storageVolumes = StorageVolumesInfo{};
        moduleInfo.turretType = "Projectile";
        moduleInfo.turretTypeVal = def::TurretType::Projectile;
        moduleInfo.turretDamageType = "Kinetic";
        moduleInfo.turretDamageTypeVal = def::DamageType::Kinetic;
        moduleInfo.turretNumBarrels = "0";
        moduleInfo.turretNumBarrelsVal = 0;
        const YAML::Node dataNode = modNode["data"];
        if (dataNode && dataNode.IsMap())
        {
            if (dataNode["max-thrust"])
            {
                moduleInfo.maxThrustVal = dataNode["max-thrust"].as<float>();
                formatModdingThrustMNString(moduleInfo.maxThrustVal,
                                            moduleInfo.maxThrust);
            }
            loadModuleStorageVolumesFromYaml(dataNode,
                                             moduleInfo.storageVolumes);
            if (dataNode["max-ship-class"])
            {
                moduleInfo.hangarMaxShipClass =
                    dataNode["max-ship-class"].as<string>();
                if (auto sc = magic_enum::enum_cast<def::ShipClass>(
                        moduleInfo.hangarMaxShipClass);
                    sc.has_value())
                {
                    moduleInfo.hangarMaxShipClassVal = sc.value();
                }
            }
            if (dataNode["hangar-space"])
            {
                moduleInfo.hangarSpaceVal =
                    dataNode["hangar-space"].as<float>();
                floatToString(
                    moduleInfo.hangarSpaceVal, moduleInfo.hangarSpace, 2);
            }
            if (dataNode[kTurretTypeYamlKey])
            {
                moduleInfo.turretType =
                    dataNode[kTurretTypeYamlKey].as<string>();
            }
            if (auto tt = magic_enum::enum_cast<def::TurretType>(
                    moduleInfo.turretType);
                tt.has_value())
            {
                moduleInfo.turretTypeVal = tt.value();
            }
            if (dataNode[kTurretDamageTypeYamlKey])
            {
                moduleInfo.turretDamageType =
                    dataNode[kTurretDamageTypeYamlKey].as<string>();
            }
            if (auto dt = magic_enum::enum_cast<def::DamageType>(
                    moduleInfo.turretDamageType);
                dt.has_value())
            {
                moduleInfo.turretDamageTypeVal = dt.value();
            }
            if (dataNode["proj-dmg"])
            {
                moduleInfo.turretProjDmgVal = dataNode["proj-dmg"].as<float>();
                floatToString(
                    moduleInfo.turretProjDmgVal, moduleInfo.turretProjDmg, 2);
            }
            if (dataNode["exit-speed"])
            {
                moduleInfo.turretExitSpeedVal =
                    dataNode["exit-speed"].as<float>();
                floatToString(moduleInfo.turretExitSpeedVal,
                              moduleInfo.turretExitSpeed,
                              2);
            }
            if (dataNode["lifetime"])
            {
                moduleInfo.turretLifetimeVal = dataNode["lifetime"].as<float>();
                floatToString(
                    moduleInfo.turretLifetimeVal, moduleInfo.turretLifetime, 2);
            }
            if (dataNode["reload-time"])
            {
                moduleInfo.turretReloadTimeVal =
                    dataNode["reload-time"].as<float>();
                floatToString(moduleInfo.turretReloadTimeVal,
                              moduleInfo.turretReloadTime,
                              2);
            }
            if (dataNode["dps"])
            {
                moduleInfo.turretDpsVal = dataNode["dps"].as<float>();
                floatToString(moduleInfo.turretDpsVal, moduleInfo.turretDps, 2);
            }
            if (dataNode["beam-width"])
            {
                moduleInfo.turretBeamWidthVal =
                    dataNode["beam-width"].as<float>();
                floatToString(moduleInfo.turretBeamWidthVal,
                              moduleInfo.turretBeamWidth,
                              2);
            }
            if (dataNode["beam-length"])
            {
                moduleInfo.turretBeamLengthVal =
                    dataNode["beam-length"].as<float>();
                floatToString(moduleInfo.turretBeamLengthVal,
                              moduleInfo.turretBeamLength,
                              2);
            }
            if (dataNode["arc-angle"])
            {
                moduleInfo.turretArcAngleVal =
                    dataNode["arc-angle"].as<float>();
                floatToString(
                    moduleInfo.turretArcAngleVal, moduleInfo.turretArcAngle, 2);
            }
            if (dataNode["arc-length"])
            {
                moduleInfo.turretArcLengthVal =
                    dataNode["arc-length"].as<float>();
                floatToString(moduleInfo.turretArcLengthVal,
                              moduleInfo.turretArcLength,
                              2);
            }
            const YAML::Node exitsNode = dataNode[kTurretExitsYamlKey];
            if (exitsNode && exitsNode.IsSequence())
            {
                for (const YAML::Node& en : exitsNode)
                {
                    if (!en || !en.IsSequence() || en.size() < 2)
                    {
                        continue;
                    }
                    TurretExitInfo te;
                    te.xVal = en[0].as<float>();
                    te.yVal = en[1].as<float>();
                    floatToString(te.xVal, te.x, 2);
                    floatToString(te.yVal, te.y, 2);
                    turretExits.push_back(std::move(te));
                }
            }
        }

        string texKey;
        if (modNode["textures"])
        {
            texKey = modNode["textures"].as<string>();
        }
        if (texKey.empty())
        {
            texKey = moduleMapKey;
        }

        TextureYamlLoadOptions texOpts;
        texOpts.yamlTextureBoundsBleed = yamlTextureBoundsBleed;
        loadTexturesFromRootBundle(root, texKey, textures, texOpts);
    }
    catch (const YAML::Exception& e)
    {
        LG_W("Modding tools: error parsing module in {}: {}", path, e.what());
        textures.clear();
        moduleInfo = ModuleInfo{};
        genInfo = GeneralInfo{};
        activeMode = ModdingToolsMode::None;
        syncModeToRml();
        return false;
    }

    resetNewTexturePickerState();
    parseEditorNumericFields();
    return true;
}

bool ModdingTools::saveModuleDataToPath(const string& path)
{
    parseEditorNumericFields();

    const string key =
        sanitizeHullKey(genInfo.name.empty() ? string("module") : genInfo.name);
    const string displayName = genInfo.name.empty() ? key : genInfo.name;

    const YAML::Node texBundle = buildTexturesYamlBundle(textures);

    YAML::Node modNode;
    modNode["name"] = displayName;
    modNode["slot-type"] = moduleInfo.slotType;
    modNode["type"] = moduleInfo.moduleType;
    modNode["textures"] = key;
    modNode["mass"] = moduleInfo.massVal;
    if (!moduleInfo.description.empty())
    {
        modNode["description"] = moduleInfo.description;
    }

    YAML::Node dataNode(YAML::NodeType::Map);
    writeModuleDataYaml(dataNode, moduleInfo);
    if (moduleInfo.moduleTypeVal == gobj::ModuleType::Turret
        && !turretExits.empty())
    {
        YAML::Node exitsNode(YAML::NodeType::Sequence);
        for (const auto& te : turretExits)
        {
            YAML::Node p(YAML::NodeType::Sequence);
            p.push_back(te.xVal);
            p.push_back(te.yVal);
            exitsNode.push_back(p);
        }
        dataNode[kTurretExitsYamlKey] = exitsNode;
    }
    if (dataNode.size() > 0)
    {
        modNode["data"] = dataNode;
    }

    YAML::Node root;
    root[kYamlTextureBoundsBleedKey] = true;
    root["textures"] = YAML::Node(YAML::NodeType::Map);
    root["textures"][key] = texBundle;
    root["module"] = YAML::Node(YAML::NodeType::Map);
    root["module"][key] = modNode;

    std::ofstream out(path);
    if (!out)
    {
        return false;
    }
    out << root;
    return out.good();
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

        stationPartInfo.storageVolumes = StorageVolumesInfo{};
        const YAML::Node dataNode = partNode["data"];
        if (dataNode && dataNode.IsMap())
        {
            loadStationStorageVolumesFromYaml(dataNode,
                                              stationPartInfo.storageVolumes);
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

        TextureYamlLoadOptions texOpts;
        texOpts.yamlTextureBoundsBleed = yamlTextureBoundsBleed;
        texOpts.skipStationConnector = true;
        loadTexturesFromRootBundle(root, texKey, textures, texOpts);
        if (!texKey.empty() && textures.empty())
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
            loadColliderVerticesFromEntry(
                colliderRoot[colKey], collider, genInfo.colliderRestitutionVal);
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

    const YAML::Node texBundle = buildTexturesYamlBundle(textures);

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
            writeStationStorageVolumesToYaml(dataNode,
                                             stationPartInfo.storageVolumes);
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

    const YAML::Node texBundle = buildTexturesYamlBundle(textures);

    YAML::Node hullNode;
    hullNode["name"] = displayName;
    hullNode["hullpoints"] = hpVal;
    hullNode["mass"] = genInfo.massVal;
    hullNode["internal-gyro-torque"] = genInfo.internalGyroTorqueVal;
    writeModuleStorageVolumesToYaml(hullNode, genInfo.storageVolumes);
    hullNode["collider"] = key;
    hullNode["textures"] = key;

    YAML::Node slotSeq(YAML::NodeType::Sequence);
    for (const auto& s : slots)
    {
        YAML::Node sn;
        sn["mod-type"] = s.slotType;
        sn["pos"] = YAML::Node(YAML::NodeType::Sequence);
        sn["pos"].push_back(s.posXVal);
        sn["pos"].push_back(s.posYVal);
        sn["rot"] = s.rotVal;
        sn["z"] = 0;
        if (gobj::moduleSlotTypeHasAngleLimits(s.slotTypeVal))
        {
            sn["min-angle"] = s.minAngleVal;
            sn["max-angle"] = s.maxAngleVal;
        }
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
