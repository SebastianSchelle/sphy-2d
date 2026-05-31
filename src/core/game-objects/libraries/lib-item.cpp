#include <lib-item.hpp>
#include <mod-manager.hpp>

namespace gobj
{

Item Item::fromYaml(const YAML::Node& node, mod::ResourceMap& resourceMap)
{
    Item item;
    TRY_YAML_DICT(item.name, node["name"], item.name);
    TRY_YAML_DICT(
        item.description, node["description"], item.description);
    string typeStr = string(magic_enum::enum_name(item.type));
    TRY_YAML_DICT(typeStr, node["type"], typeStr);
    item.type = magic_enum::enum_cast<ItemType>(typeStr).value_or(ItemType::Ore);
    string storageTypeStr = string(magic_enum::enum_name(item.storageType));
    TRY_YAML_DICT(storageTypeStr, node["storage-type"], storageTypeStr);
    item.storageType = magic_enum::enum_cast<ItemStorageType>(storageTypeStr)
                           .value_or(ItemStorageType::Cargo);
    TRY_YAML_DICT(item.volume, node["volume"], item.volume);
    TRY_YAML_DICT(item.density, node["density"], item.density);
    if (node["price-range"] && node["price-range"].IsMap())
    {
        TRY_YAML_DICT(
            item.priceRange.min, node["price-range"]["min"], item.priceRange.min);
        TRY_YAML_DICT(
            item.priceRange.max, node["price-range"]["max"], item.priceRange.max);
    }
    string texName = "";
    TRY_YAML_DICT(texName, node["world-textures"], "");
    mod::MappedTextureHandle mTexHandle =
        resourceMap.getTextureHandle(texName);
    if (!mTexHandle.isValid())
    {
        LG_W("Texture not found: {}", texName);
        item.worldTexture = GenericHandle::Invalid();
    }
    else
    {
        item.worldTexture = *(GenericHandle*)&mTexHandle;
    }
    texName = "";
    TRY_YAML_DICT(texName, node["icon-textures"], "");
    mTexHandle = resourceMap.getTextureHandle(texName);
    if (!mTexHandle.isValid())
    {
        LG_W("Texture not found: {}", texName);
        item.iconTexture = GenericHandle::Invalid();
    }
    else
    {
        item.iconTexture = *(GenericHandle*)&mTexHandle;
    }

    return item;
}

}  // namespace gobj
