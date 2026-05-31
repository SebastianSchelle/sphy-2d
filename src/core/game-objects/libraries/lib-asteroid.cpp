#include <lib-asteroid.hpp>
#include <logging.hpp>

namespace gobj
{

void Asteroid::loadDebrisFromYaml(Asteroid& asteroid,
                                  const YAML::Node& node,
                                  const con::ItemLib<Asteroid>& asteroidLib)
{
    asteroid.debris.clear();
    const YAML::Node debrisNode = node["debris"];
    if (!debrisNode || !debrisNode.IsMap())
    {
        return;
    }
    for (const auto& entry : debrisNode)
    {
        if (!entry.first.IsScalar())
        {
            continue;
        }
        const string asteroidName = entry.first.as<string>();
        float weightRaw = 0.0f;
        TRY_YAML_DICT(weightRaw, entry.second, weightRaw);
        const uint8_t weight = static_cast<uint8_t>(
            std::clamp<int>(static_cast<int>(std::lround(weightRaw)), 0, 255));
        const AsteroidHandle debrisHandle = asteroidLib.getHandle(asteroidName);
        if (!debrisHandle.isValid())
        {
            LG_W("Asteroid not found for debris: {}", asteroidName);
            continue;
        }
        asteroid.debris.emplace_back(debrisHandle, weight);
    }
}

Asteroid Asteroid::fromYaml(const YAML::Node& node,
                             const con::ItemLib<gobj::Item>& itemLib,
                             const con::ItemLib<gobj::Textures>& texturesLib,
                             const con::ItemLib<gobj::Collider>& colliderLib,
                             const con::ItemLib<gobj::Asteroid>& asteroidLib)
{
    Asteroid asteroid;
    TRY_YAML_DICT(asteroid.name, node["name"], asteroid.name);
    TRY_YAML_DICT(
        asteroid.description, node["description"], asteroid.description);
    TRY_YAML_DICT(asteroid.mass, node["mass"], asteroid.mass);
    TRY_YAML_DICT(asteroid.maxHp, node["max-hp"], asteroid.maxHp);
    string texturesName = "";
    TRY_YAML_DICT(texturesName, node["textures"], "");
    if (texturesName != "")
    {
        asteroid.textures = texturesLib.getHandle(texturesName);
    }
    string colliderName = "";
    TRY_YAML_DICT(colliderName, node["collider"], "");
    if (colliderName != "")
    {
        asteroid.collider = colliderLib.getHandle(colliderName);
    }
    const YAML::Node compositionNode = node["composition"];
    if (compositionNode && compositionNode.IsMap())
    {
        for (const auto& entry : compositionNode)
        {
            if (!entry.first.IsScalar())
            {
                continue;
            }
            const string itemName = entry.first.as<string>();
            float fraction = 0.0f;
            TRY_YAML_DICT(fraction, entry.second, fraction);
            const ItemHandle itemHandle = itemLib.getHandle(itemName);
            if (!itemHandle.isValid())
            {
                LG_W("Item not found for asteroid composition: {}", itemName);
                continue;
            }
            asteroid.composition.emplace_back(itemHandle, fraction);
        }
    }
    loadDebrisFromYaml(asteroid, node, asteroidLib);
    return asteroid;
}

}  // namespace gobj
