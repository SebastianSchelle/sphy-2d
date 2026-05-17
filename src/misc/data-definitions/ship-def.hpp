#ifndef SHIP_DEF_HPP
#define SHIP_DEF_HPP

#include <std-inc.hpp>
#include <magic_enum/magic_enum.hpp>

namespace def
{

enum class ShipClass : uint8_t
{
    Drone,
    Spark,
    Echo,
    Wyrm,
    Kraken,
    Leviathan,
    Behemoth,
    Colossus,
    Titan,
    NumShipClasses,
};

constexpr float
    ShipClassMaxLength[static_cast<size_t>(ShipClass::NumShipClasses)] = {
        3.0f,      // Drone
        20.0f,     // Spark
        50.0f,     // Echo
        10000.0f,  // Wyrm
        10000.0f,  // Kraken
        10000.0f,  // Leviathan
        10000.0f,  // Behemoth
        10000.0f,  // Colossus
        10000.0f,  // Titan
};

constexpr float
    ShipClassMaxWidth[static_cast<size_t>(ShipClass::NumShipClasses)] = {
        3.0f,      // Drone
        20.0f,     // Spark
        20.0f,     // Echo
        10000.0f,  // Wyrm
        10000.0f,  // Kraken
        10000.0f,  // Leviathan
        10000.0f,  // Behemoth
        10000.0f,  // Colossus
        10000.0f,  // Titan
};

constexpr float
    ShipClassHangarSpace[static_cast<size_t>(ShipClass::NumShipClasses)] = {
        3.0f,      // Drone
        20.0f,     // Spark
        50.0f,     // Echo
        10000.0f,  // Wyrm
        10000.0f,  // Kraken
        10000.0f,  // Leviathan
        10000.0f,  // Behemoth
        10000.0f,  // Colossus
        10000.0f,  // Titan
};

}  // namespace def


EXT_FMT(def::ShipClass, "{}", magic_enum::enum_name(o));

#endif  // SHIP_DEF_HPP