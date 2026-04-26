#include <lib-textures.hpp>
#include <mod-manager.hpp>

namespace gobj
{

Textures Textures::fromYaml(const YAML::Node& node, mod::ResourceMap& resourceMap)
{
    Textures textures;
    TRY_YAML_DICT(textures.name, node["name"], "");
    TRY_YAML_DICT(textures.description, node["description"], "");
    for (const auto& texNode : node["textures"])
    {
        Texture texture;
        string texName = "";
        TRY_YAML_DICT(texName, texNode["name"], "");
        mod::MappedTextureHandle mTexHandle =
            resourceMap.getTextureHandle(texName);
        if (!mTexHandle.isValid())
        {
            LG_W("Texture not found: {}", texName);
            texture.texHandle = *(GenericHandle*)&mTexHandle;
        }
        else
        {
            texture.texHandle = *(GenericHandle*)&mTexHandle;
        }
        TRY_YAML_DICT(texture.bounds.x, texNode["bounds"][0], 0.0f);
        TRY_YAML_DICT(texture.bounds.y, texNode["bounds"][1], 0.0f);
        TRY_YAML_DICT(texture.bounds.z, texNode["bounds"][2], 1.0f);
        TRY_YAML_DICT(texture.bounds.w, texNode["bounds"][3], 1.0f);
        TRY_YAML_DICT(texture.zIndex, texNode["zIndex"], 0);
        uint8_t flags = 0;
        TRY_YAML_DICT(flags, texNode["flags"], 0);
        texture.flags = static_cast<TextureFlags>(flags);
        TRY_YAML_DICT(texture.rot, texNode["rot"], 0.0f);
        texture.rot = smath::degToRad(texture.rot);
        textures.textures.push_back(texture);
    }
    return textures;
}

MapIcon MapIcon::fromYaml(const YAML::Node& node, mod::ResourceMap& resourceMap)
{
    MapIcon mapIcon;
    string name;
    TRY_YAML_DICT(mapIcon.name, node["name"], "");
    TRY_YAML_DICT(mapIcon.description, node["description"], "");
    string texName = "";
    TRY_YAML_DICT(texName, node["texName"], "");
    mod::MappedTextureHandle mTexHandle =
        resourceMap.getTextureHandle(texName);
    if (!mTexHandle.isValid())
    {
        LG_W("Texture not found: {}", texName);
        mapIcon.texHandle = *(GenericHandle*)&mTexHandle;
    }
    else
    {
        mapIcon.texHandle = *(GenericHandle*)&mTexHandle;
    }
    TRY_YAML_DICT(mapIcon.size.x, node["size"][0], 0.0f);
    TRY_YAML_DICT(mapIcon.size.y, node["size"][1], 0.0f);
    return mapIcon;
}

}  // namespace gobj