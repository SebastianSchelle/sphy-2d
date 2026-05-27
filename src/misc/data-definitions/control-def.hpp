#ifndef CONTROL_DEF_HPP
#define CONTROL_DEF_HPP

#include <std-inc.hpp>

namespace def
{

struct ThirdPersonControl
{
    using ControlFlags = uint32_t;
    static constexpr ControlFlags FLG_FIRE_WEAPONS = 1 << 0;
    static constexpr ControlFlags FLG_DRIVE_MANUAL = 1 << 1;
    static constexpr ControlFlags FLG_TURN_MANUAL = 1 << 2;

    vec2 thrust = vec2(0.0f, 0.0f);
    vec2 ptrPos = vec2(0.0f, 0.0f);
    float torque = 0.0f;
    ControlFlags flags = 0;

    void acc()
    {
        LG_D("Acc");
        thrust.y = 1.0f;
        flags |= FLG_DRIVE_MANUAL;
    }

    void stopAcc()
    {
        if (thrust.y > 0.0f)
        {
            thrust.y = 0.0f;
        }
    }

    void dec()
    {
        thrust.y = -1.0f;
        flags |= FLG_DRIVE_MANUAL;
    }

    void stopDec()
    {
        if (thrust.y < 0.0f)
        {
            thrust.y = 0.0f;
        }
    }

    void rotateCCW()
    {
        torque = -1.0f;
        flags |= FLG_TURN_MANUAL;
    }

    void stopRotateCCW()
    {
        if (torque < 0.0f)
        {
            torque = 0.0f;
        }
    }

    void rotateCW()
    {
        torque = 1.0f;
        flags |= FLG_TURN_MANUAL;
    }

    void stopRotateCW()
    {
        if (torque > 0.0f)
        {
            torque = 0.0f;
        }
    }

    void strafeLeft()
    {
        thrust.x = 1.0f;
        flags |= FLG_DRIVE_MANUAL;
    }

    void stopStrafeLeft()
    {
        if (thrust.x > 0.0f)
        {
            thrust.x = 0.0f;
        }
    }

    void strafeRight()
    {
        thrust.x = -1.0f;
        flags |= FLG_DRIVE_MANUAL;
    }

    void stopStrafeRight()
    {
        if (thrust.x < 0.0f)
        {
            thrust.x = 0.0f;
        }
    }
};

#define SER_THIRD_PERSON_CONTROL                                               \
    s.value4b(o.flags);                                                        \
    SOBJ(o.thrust);                                                            \
    s.value4b(o.torque);                                                       \
    s.object(o.ptrPos);
EXT_SER(ThirdPersonControl, SER_THIRD_PERSON_CONTROL)
EXT_DES(ThirdPersonControl, SER_THIRD_PERSON_CONTROL)

}  // namespace def

EXT_FMT(def::ThirdPersonControl, "thrust: {}, torque: {}, ptrPos: {}", o.thrust, o.torque, o.ptrPos);

#endif