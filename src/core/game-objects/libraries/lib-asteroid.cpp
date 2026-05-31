#include <lib-asteroid.hpp>
#include <logging.hpp>
#include <cmath>

namespace gobj
{

namespace
{

float asteroidVolumeFromCollider(const Collider* collider)
{
    if (collider == nullptr || collider->vertices.empty())
    {
        return 0.0f;
    }
    const vec2 ext = smath::colliderLocalExtents(collider->vertices);
    const float r = 0.25f * (ext.x + ext.y);
    return (4.0f / 3.0f) * static_cast<float>(M_PI) * r * r * r;
}

void loadCompositionFromYaml(
    const YAML::Node& compositionNode,
    const con::ItemLib<gobj::Item>& itemLib,
    AsteroidComposition& composition)
{
    composition.clear();
    if (!compositionNode || !compositionNode.IsMap())
    {
        return;
    }
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
        composition.emplace_back(itemHandle, fraction);
    }
}

}  // namespace

void Asteroid::loadChildrenFromYaml(
    Asteroid& asteroid,
    const YAML::Node& node,
    const con::ItemLib<Asteroid>& asteroidLib)
{
    if (asteroid.type != AsteroidType::Parent)
    {
        return;
    }
    auto* parent =
        std::get_if<AsteroidParentdata>(&asteroid.content);
    if (parent == nullptr)
    {
        return;
    }
    parent->children.clear();
    const YAML::Node childrenNode = node["children"];
    if (!childrenNode || !childrenNode.IsMap())
    {
        return;
    }
    for (const auto& entry : childrenNode)
    {
        if (!entry.first.IsScalar())
        {
            continue;
        }
        const string asteroidName = entry.first.as<string>();
        float countRaw = 0.0f;
        TRY_YAML_DICT(countRaw, entry.second, countRaw);
        const uint8_t count = static_cast<uint8_t>(
            std::clamp<int>(static_cast<int>(std::lround(countRaw)), 0, 255));
        if (count == 0)
        {
            continue;
        }
        const AsteroidHandle childHandle = asteroidLib.getHandle(asteroidName);
        if (!childHandle.isValid())
        {
            LG_W("Asteroid not found for child: {}", asteroidName);
            continue;
        }
        parent->children.emplace_back(childHandle, count);
    }
}

Asteroid Asteroid::fromYaml(const YAML::Node& node,
                            const con::ItemLib<gobj::Item>& itemLib,
                            const con::ItemLib<gobj::Textures>& texturesLib,
                            con::ItemLib<gobj::Collider>& colliderLib)
{
    Asteroid asteroid;
    TRY_YAML_DICT(asteroid.name, node["name"], asteroid.name);
    TRY_YAML_DICT(
        asteroid.description, node["description"], asteroid.description);
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

    string typeStr = string(magic_enum::enum_name(asteroid.type));
    TRY_YAML_DICT(typeStr, node["type"], typeStr);
    asteroid.type =
        magic_enum::enum_cast<AsteroidType>(typeStr)
            .value_or(AsteroidType::Fragment);

    if (asteroid.type == AsteroidType::Parent)
    {
        asteroid.content = AsteroidParentdata{};
    }
    else
    {
        AsteroidFragmentdata fragment{};
        loadCompositionFromYaml(node["composition"], itemLib, fragment.composition);
        asteroid.content = std::move(fragment);
    }

    const Collider* colliderData =
        asteroid.collider.isValid() ? colliderLib.getItem(asteroid.collider)
                                    : nullptr;
    asteroid.volume = asteroidVolumeFromCollider(colliderData);

    return asteroid;
}

}  // namespace gobj
