#ifndef CONTROL_DEF_HPP
#define CONTROL_DEF_HPP

#include <std-inc.hpp>

namespace def
{

struct ThirdPersonControl
{
    vec2 thrust = vec2(0.0f, 0.0f);
    float torque = 0.0f;
    bool dirty_active = false;

    void acc()
    {
        LG_D("Acc");
        thrust.y = 1.0f;
        dirty_active = true;
    }

    void stopAcc()
    {
        if (thrust.y > 0.0f)
        {
            thrust.y = 0.0f;
            dirty_active = true;
        }
    }

    void dec()
    {
        thrust.y = -1.0f;
        dirty_active = true;
    }

    void stopDec()
    {
        if (thrust.y < 0.0f)
        {
            thrust.y = 0.0f;
            dirty_active = true;
        }
    }

    void rotateCCW()
    {
        torque = -1.0f;
        dirty_active = true;
    }

    void stopRotateCCW()
    {
        if (torque < 0.0f)
        {
            torque = 0.0f;
            dirty_active = true;
        }
    }

    void rotateCW()
    {
        torque = 1.0f;
        dirty_active = true;
    }

    void stopRotateCW()
    {
        if (torque > 0.0f)
        {
            torque = 0.0f;
            dirty_active = true;
        }
    }

    void strafeLeft()
    {
        thrust.x = 1.0f;
        dirty_active = true;
    }

    void stopStrafeLeft()
    {
        if (thrust.x > 0.0f)
        {
            thrust.x = 0.0f;
            dirty_active = true;
        }
    }

    void strafeRight()
    {
        thrust.x = -1.0f;
        dirty_active = true;
    }

    void stopStrafeRight()
    {
        if (thrust.x < 0.0f)
        {
            thrust.x = 0.0f;
            dirty_active = true;
        }
    }
};

#define SER_THIRD_PERSON_CONTROL                                               \
    SOBJ(o.thrust);                                                            \
    s.value4b(o.torque);
EXT_SER(ThirdPersonControl, SER_THIRD_PERSON_CONTROL)
EXT_DES(ThirdPersonControl, SER_THIRD_PERSON_CONTROL)

}  // namespace def

EXT_FMT(def::ThirdPersonControl, "thrust: {}, torque: {}", o.thrust, o.torque);

#endif