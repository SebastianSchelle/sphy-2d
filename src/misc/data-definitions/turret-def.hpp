#ifndef TURRET_DEF_HPP
#define TURRET_DEF_HPP

#include <std-inc.hpp>
#include <magic_enum/magic_enum.hpp>

namespace def
{

/** How damage interacts with shields, armor, and systems. */
enum class DamageType : uint8_t
{
    Kinetic,
    Thermal,
    Energy,
    Explosive,
    Ion,    
    NumDamageTypes,
};

/** Ship-mounted weapon archetype (projectile / beam / missile family). */
enum class TurretType : uint8_t
{
    Projectile, // guns, flak, plasma, etc.
    Laser,      // beams, pulse, particle lances
    Arc,        // tesla coils, firing in a wide arc area
    Missile,    // guided warheads, torpedoes
    Railgun,    // rails, mass drivers, need to charge up
    Mining,     // drills, salvagers
    NumTurretTypes,
};

constexpr DamageType
    TurretTypeDefaultDamage[static_cast<size_t>(TurretType::NumTurretTypes)] = {
        DamageType::Kinetic,     // Projectile
        DamageType::Energy,      // Laser
        DamageType::Thermal,     // Plasma
        DamageType::Explosive,   // Missile
        DamageType::Kinetic,     // Railgun
        DamageType::Kinetic,     // Mining
};

}  // namespace def

EXT_FMT(def::DamageType, "{}", magic_enum::enum_name(o));
EXT_FMT(def::TurretType, "{}", magic_enum::enum_name(o));

#endif  // TURRET_DEF_HPP
