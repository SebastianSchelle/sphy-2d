#include <lib-modules.hpp>

namespace gobj
{

namespace mdata
{

namespace
{

constexpr const char* kStorageVolumeYamlKeys[static_cast<size_t>(
    StorageType::NumStorageTypes)] = {
    "volume-container-s",
    "volume-container-l",
    "volume-tank",
    "volume-bulk",
};

ProjectileHandle resolveProjectileHandle(
    const string& key, const con::ItemLib<gobj::Projectile>& projectileLib)
{
    if (key.empty())
    {
        LG_E("Projectile key is empty");
        return ProjectileHandle::Invalid();
    }
    auto handle = projectileLib.getHandle(key);
    if (!handle.isValid())
    {
        LG_E("Projectile {} not found", key);
        return ProjectileHandle::Invalid();
    }
    return handle;
}

MissileHandle resolveMissileHandle(const string& key,
                                   const con::ItemLib<gobj::Missile>& missileLib)
{
    if (key.empty())
    {
        LG_E("Missile key is empty");
        return MissileHandle::Invalid();
    }
    auto handle = missileLib.getHandle(key);
    if (!handle.isValid())
    {
        LG_E("Missile {} not found", key);
        return MissileHandle::Invalid();
    }
    return handle;
}

}  // namespace

MainThruster MainThruster::fromYaml(const YAML::Node& node)
{
    MainThruster mainThruster;
    TRY_YAML_DICT(mainThruster.maxThrust, node["max-thrust"], 100.0f);
    return mainThruster;
}

ManeuverThruster ManeuverThruster::fromYaml(const YAML::Node& node)
{
    ManeuverThruster maneuverThruster;
    TRY_YAML_DICT(maneuverThruster.maxThrust, node["max-thrust"], 100.0f);
    return maneuverThruster;
}

Storage Storage::fromYaml(const YAML::Node& node)
{
    Storage storage{};
    for (size_t i = 0; i < static_cast<size_t>(StorageType::NumStorageTypes); ++i)
    {
        storage.volume[i] = 0.0f;
    }

    bool anyPerType = false;
    for (size_t i = 0; i < static_cast<size_t>(StorageType::NumStorageTypes); ++i)
    {
        if (node[kStorageVolumeYamlKeys[i]])
        {
            anyPerType = true;
            float parsed = 0.0f;
            TRY_YAML_DICT(parsed, node[kStorageVolumeYamlKeys[i]], 0.0f);
            storage.volume[i] = parsed;
        }
    }

    return storage;
}

Hangar Hangar::fromYaml(const YAML::Node& node)
{
    Hangar hangar{};
    string shipClassStr = "Drone";
    TRY_YAML_DICT(shipClassStr, node["max-ship-class"], "Drone");
    hangar.maxShipClass =
        magic_enum::enum_cast<def::ShipClass>(shipClassStr)
            .value_or(def::ShipClass::Drone);
    TRY_YAML_DICT(hangar.hangarSpace, node["hangar-space"], 0.0f);
    return hangar;
}

Turret Turret::fromYaml(const YAML::Node& node,
                        const con::ItemLib<gobj::Projectile>& projectileLib,
                        const con::ItemLib<gobj::Missile>& missileLib)
{
    Turret turret{};
    string turretTypeStr = string(magic_enum::enum_name(turret.type));
    TRY_YAML_DICT(turretTypeStr, node["turret-type"], turretTypeStr);
    turret.type = magic_enum::enum_cast<def::TurretType>(turretTypeStr)
                      .value_or(def::TurretType::Projectile);

    string damageTypeStr = string(magic_enum::enum_name(
        def::TurretTypeDefaultDamage[static_cast<size_t>(turret.type)]));
    TRY_YAML_DICT(damageTypeStr, node["damage-type"], damageTypeStr);
    turret.damageType = magic_enum::enum_cast<def::DamageType>(damageTypeStr)
                            .value_or(def::TurretTypeDefaultDamage[static_cast<
                                size_t>(turret.type)]);

    const YAML::Node exitsNode = node["barrel-exits"];
    if (exitsNode && exitsNode.IsSequence())
    {
        for (const YAML::Node& en : exitsNode)
        {
            if (!en || !en.IsSequence() || en.size() < 2)
            {
                continue;
            }
            turret.barrelExits.emplace_back(en[0].as<float>(), en[1].as<float>());
        }
    }

    if (turret.barrelExits.empty())
    {
        turret.barrelExits.emplace_back(0.0f, 0.0f);
    }
    turret.numBarrels = static_cast<uint8_t>(
        std::min<size_t>(255, turret.barrelExits.size()));
    TRY_YAML_DICT(turret.rotSpeed, node["rot-speed"], turret.rotSpeed);

    string projectileKey;
    TRY_YAML_DICT(projectileKey, node["projectile"], "");
    string missileKey;
    TRY_YAML_DICT(missileKey, node["missile"], "");

    switch (turret.type)
    {
        case def::TurretType::Projectile:
        {
            ProjectileData projectile{};
            TRY_YAML_DICT(
                projectile.projDmg, node["proj-dmg"], projectile.projDmg);
            TRY_YAML_DICT(
                projectile.exitSpeed, node["exit-speed"], projectile.exitSpeed);
            TRY_YAML_DICT(
                projectile.reloadTime,
                node["reload-time"],
                projectile.reloadTime);
            projectile.projectile =
                resolveProjectileHandle(projectileKey, projectileLib);
            turret.data = projectile;
            break;
        }
        case def::TurretType::Railgun:
        {
            RailgunData railgun{};
            TRY_YAML_DICT(railgun.projDmg, node["proj-dmg"], railgun.projDmg);
            TRY_YAML_DICT(
                railgun.exitSpeed, node["exit-speed"], railgun.exitSpeed);
            railgun.projectile =
                resolveProjectileHandle(projectileKey, projectileLib);
            turret.data = railgun;
            break;
        }
        case def::TurretType::Missile:
        {
            MissileData missile{};
            missile.missile = resolveMissileHandle(missileKey, missileLib);
            turret.data = missile;
            break;
        }
        case def::TurretType::Laser:
        {
            LaserData laser{};
            TRY_YAML_DICT(laser.dps, node["dps"], laser.dps);
            TRY_YAML_DICT(laser.beamWidth, node["beam-width"], laser.beamWidth);
            TRY_YAML_DICT(laser.beamLength, node["beam-length"], laser.beamLength);
            TRY_YAML_DICT(laser.beamColor, node["beam-color"], laser.beamColor);
            turret.data = laser;
            break;
        }
        case def::TurretType::Arc:
        {
            ArcData arc{};
            TRY_YAML_DICT(arc.dps, node["dps"], arc.dps);
            TRY_YAML_DICT(arc.arcAngle, node["arc-angle"], arc.arcAngle);
            TRY_YAML_DICT(arc.arcLength, node["arc-length"], arc.arcLength);
            TRY_YAML_DICT(arc.arcColor, node["arc-color"], arc.arcColor);
            turret.data = arc;
            break;
        }
        default:
            break;
    }
    return turret;
}

}  // namespace mdata

Module Module::fromYaml(const YAML::Node& node,
                        const con::ItemLib<gobj::Textures>& texturesLib,
                        const con::ItemLib<gobj::Projectile>& projectileLib,
                        const con::ItemLib<gobj::Missile>& missileLib)
{
    Module module;
    TRY_YAML_DICT(module.name, node["name"], "");
    TRY_YAML_DICT(module.description, node["description"], "");
    string texturesName = "";
    TRY_YAML_DICT(texturesName, node["textures"], "");
    if (texturesName != "")
    {
        module.textures = texturesLib.getHandle(texturesName);
    }
    string baseTextureName = "";
    TRY_YAML_DICT(baseTextureName, node["base-texture"], "");
    if (baseTextureName != "")
    {
        module.texturesBase = texturesLib.getHandle(baseTextureName);
    }
    string slotTypeStr = "";
    TRY_YAML_DICT(slotTypeStr, node["slot-type"], "");
    module.slotType =
        magic_enum::enum_cast<ModuleSlotType>(slotTypeStr).value();
    string typeStr = "";
    TRY_YAML_DICT(typeStr, node["type"], "");
    module.type = magic_enum::enum_cast<ModuleType>(typeStr).value();
    TRY_YAML_DICT(module.mass, node["mass"], 1.0f);

    const auto& dataNode = node["data"];
    if (dataNode && dataNode.IsMap())
    {
        switch (module.type)
        {
            case ModuleType::MainThruster:
                module.data = mdata::MainThruster::fromYaml(dataNode);
                break;
            case ModuleType::ManeuverThruster:
                module.data = mdata::ManeuverThruster::fromYaml(dataNode);
                break;
            case ModuleType::Storage:
                module.data = mdata::Storage::fromYaml(dataNode);
                break;
            case ModuleType::Hangar:
                module.data = mdata::Hangar::fromYaml(dataNode);
                break;
            case ModuleType::Turret:
                module.data =
                    mdata::Turret::fromYaml(dataNode, projectileLib, missileLib);
                break;
            default:
                break;
        }
    }
    return module;
}

}  // namespace gobj
