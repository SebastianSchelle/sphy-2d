#ifndef COMP_GFX_HPP
#define COMP_GFX_HPP

#include <entt/entt.hpp>
#include <mod-manager.hpp>
#include <std-inc.hpp>
#include <yaml-cpp/yaml.h>

namespace ecs
{

struct MapIcon
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "map-icon";

    GenericHandle mapIconHandle;
};

#define SER_MAP_ICON SOBJ(o.mapIconHandle);
EXT_SER(MapIcon, SER_MAP_ICON)
EXT_DES(MapIcon, SER_MAP_ICON)

struct Textures
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "textures";

    GenericHandle texturesHandle;
};

#define SER_TEXTURES SOBJ(o.texturesHandle);
EXT_SER(Textures, SER_TEXTURES)
EXT_DES(Textures, SER_TEXTURES)

}  // namespace ecs

EXT_FMT(ecs::MapIcon, "mapIconHandle: {}", o.mapIconHandle);
EXT_FMT(ecs::Textures, "texturesHandle: {}", o.texturesHandle);

#endif