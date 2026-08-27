#include <lib-projectile.hpp>

namespace gobj
{

Projectile Projectile::fromYaml(const YAML::Node& node,
                                const con::ItemLib<gobj::Textures>& texturesLib)
{
    Projectile projectile;
    TRY_YAML_DICT(projectile.name, node["name"], projectile.name);
    TRY_YAML_DICT(
        projectile.description, node["description"], projectile.description);
    TRY_YAML_DICT(projectile.dmg, node["dmg"], projectile.dmg);
    TRY_YAML_DICT(projectile.lifetime, node["lifetime"], projectile.lifetime);
    string damageTypeStr = string(magic_enum::enum_name(projectile.damageType));
    TRY_YAML_DICT(damageTypeStr, node["damage-type"], damageTypeStr);
    projectile.damageType =
        magic_enum::enum_cast<def::DamageType>(damageTypeStr)
            .value_or(def::DamageType::Kinetic);
    string texturesName = "";
    TRY_YAML_DICT(texturesName, node["textures"], "");
    if (texturesName != "")
    {
        projectile.textures = texturesLib.getHandle(texturesName);
    }
    return projectile;
}

Missile Missile::fromYaml(const YAML::Node& node,
                          const con::ItemLib<gobj::Textures>& texturesLib)
{
    Missile missile;
    TRY_YAML_DICT(missile.name, node["name"], missile.name);
    TRY_YAML_DICT(
        missile.description, node["description"], missile.description);
    TRY_YAML_DICT(missile.dmg, node["dmg"], missile.dmg);
    TRY_YAML_DICT(missile.detonationRadius,
                  node["detonation-radius"],
                  missile.detonationRadius);
    TRY_YAML_DICT(missile.lifetime, node["lifetime"], missile.lifetime);
    string damageTypeStr = string(magic_enum::enum_name(missile.damageType));
    TRY_YAML_DICT(damageTypeStr, node["damage-type"], damageTypeStr);
    missile.damageType = magic_enum::enum_cast<def::DamageType>(damageTypeStr)
                             .value_or(def::DamageType::Explosive);
    string texturesName = "";
    TRY_YAML_DICT(texturesName, node["textures"], "");
    if (texturesName != "")
    {
        missile.textures = texturesLib.getHandle(texturesName);
    }
    return missile;
}

Beam Beam::fromYaml(const YAML::Node& node)
{
    Beam beam;
    TRY_YAML_DICT(beam.name, node["name"], beam.name);
    TRY_YAML_DICT(
        beam.description, node["description"], beam.description);
    TRY_YAML_DICT(beam.dps, node["dps"], beam.dps);
    TRY_YAML_DICT(beam.range, node["range"], beam.range);
    TRY_YAML_DICT(beam.width, node["width"], beam.width);
    TRY_YAML_DICT(beam.color, node["color"], beam.color);
    string damageTypeStr = string(magic_enum::enum_name(beam.damageType));
    TRY_YAML_DICT(damageTypeStr, node["damage-type"], damageTypeStr);
    beam.damageType = magic_enum::enum_cast<def::DamageType>(damageTypeStr)
                             .value_or(def::DamageType::Explosive);
    return beam;
}

}  // namespace gobj
