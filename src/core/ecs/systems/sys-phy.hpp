#ifndef SYS_PHY_HPP
#define SYS_PHY_HPP

#include <algorithm>
#include <cmath>
#include <components/comp-phy.hpp>
#include <components/comp-ident.hpp>
#include <ecs.hpp>
#include <std-inc.hpp>
#include <world.hpp>

namespace ecs
{

const System sysPhyPidHold = {
    "sysPhyPidHold",
    [](entt::entity entity,
       const ecs::EntityId& entityId,
       float dt,
       std::shared_ptr<PtrHandle> ptrHandle)
    {

        // I don't like this crap
        /*
        - calculate direction vector and corresponding max thrust vector
        - from max thrust vector, distance and mass calcute max velocity (clamp with maxVel from thrust system)
        - PID control thrust with [-1,1] in x/y direction and scale with thrust
        - Use from hive2d_v2, this is exactly what i have done already / use sqrtf

        */



        // Outer: position -> desired velocity; inner: velocity error -> a_cmd,
        // F = m * a_cmd. Rotation: angle -> ω_des, then τ = I * α_cmd.
        constexpr float kPidPos = 0.4f;
        constexpr float kPidVel = 2.0f;
        constexpr float kPidVelI = 0.1f;
        constexpr float kPidVelD = 0.05f;
        constexpr float kPidVelIntClamp = 5.0f;
        constexpr float kPidRot = 2.0f;
        constexpr float kPidOmega = 3.0f;
        constexpr float kPidOmegaI = 0.5f;
        constexpr float kPidOmegaD = 0.02f;
        constexpr float kPidOmegaIntClamp = 2.0f;

        auto reg = ptrHandle->registry;
        auto* pid = reg->try_get<PhyPidHold>(entity);
        if (!pid)
        {
            return;
        }
        auto* transform = reg->try_get<Transform>(entity);
        auto* physicsBody = reg->try_get<PhysicsBody>(entity);
        auto* phyThrust = reg->try_get<PhyThrust>(entity);
        if (!transform || !physicsBody || !phyThrust)
        {
            return;
        }

        if (!pid->enabled || dt <= 1e-8f)
        {
            if (!pid->enabled)
            {
                pid->velIntegral = {};
                pid->prevVelErr = {};
                pid->rotVelIntegral = 0.0f;
                pid->prevOmegaErr = 0.0f;
                pid->prevValid = false;
            }
            return;
        }

        const vec2 pos_err = pid->posSet - transform->pos;
        vec2 vel_des = kPidPos * pos_err;
        const float vdes_len = glm::length(vel_des);
        if (vdes_len > phyThrust->maxSpd && vdes_len > 1e-8f)
        {
            vel_des *= phyThrust->maxSpd / vdes_len;
        }

        const vec2 vel_err = vel_des - physicsBody->vel;
        pid->velIntegral += vel_err * dt;
        const float vi_len = glm::length(pid->velIntegral);
        if (vi_len > kPidVelIntClamp && vi_len > 1e-8f)
        {
            pid->velIntegral *= kPidVelIntClamp / vi_len;
        }

        vec2 d_vel_err{};
        if (pid->prevValid)
        {
            d_vel_err = (vel_err - pid->prevVelErr) / dt;
        }
        pid->prevVelErr = vel_err;

        const vec2 a_cmd = kPidVel * vel_err + kPidVelI * pid->velIntegral
                           + kPidVelD * d_vel_err;
        const vec2 F_world = physicsBody->mass * a_cmd;
        phyThrust->setThrustGlobal(F_world, *transform);

        const float ang_err = hmath::angleError(pid->rotSet, transform->rot);
        float omega_des = kPidRot * ang_err;
        omega_des =
            std::clamp(omega_des, -phyThrust->maxRotVel, phyThrust->maxRotVel);

        const float omega_err = omega_des - physicsBody->rotVel;
        pid->rotVelIntegral += omega_err * dt;
        pid->rotVelIntegral =
            std::clamp(pid->rotVelIntegral,
                       -kPidOmegaIntClamp,
                       kPidOmegaIntClamp);

        float d_omega_err = 0.0f;
        if (pid->prevValid)
        {
            d_omega_err = (omega_err - pid->prevOmegaErr) / dt;
        }
        pid->prevOmegaErr = omega_err;
        pid->prevValid = true;

        const float alpha_cmd = kPidOmega * omega_err
                                + kPidOmegaI * pid->rotVelIntegral
                                + kPidOmegaD * d_omega_err;
        const float torque_cmd = physicsBody->inertia * alpha_cmd;
        phyThrust->setTorque(torque_cmd);
    }};

const System sysPhyThrust = {
    "sysPhyThrust",
    [](entt::entity entity,
       const ecs::EntityId& entityId,
       float dt,
       std::shared_ptr<PtrHandle> ptrHandle)
    {
        auto reg = ptrHandle->registry;
        auto* physicsBody = reg->try_get<PhysicsBody>(entity);
        auto* phyThrust = reg->try_get<PhyThrust>(entity);
        if (phyThrust && physicsBody)
        {
            if (physicsBody->rotVel > phyThrust->maxRotVel
                && phyThrust->torque > 0)
            {
                phyThrust->torque = 0.0;
            }
            else if (physicsBody->rotVel < -phyThrust->maxRotVel
                     && phyThrust->torque < 0)
            {
                phyThrust->torque = 0.0;
            }
            physicsBody->rotAcc += phyThrust->torque / physicsBody->inertia;

            float spd = glm::length(physicsBody->vel);
            glm::vec2 thrustApply = phyThrust->thrustGlobal;
            if (spd >= phyThrust->maxSpd && spd > 1e-8f)
            {
                const glm::vec2 vhat = physicsBody->vel / spd;
                const float along = glm::dot(phyThrust->thrustGlobal, vhat);
                if (along > 0.f)
                {
                    thrustApply -= vhat * along;
                }
            }
            physicsBody->acc += thrustApply / physicsBody->mass;

            // Reset thrust after each update cycle
            phyThrust->torque = 0.0f;
            phyThrust->thrustGlobal = vec2(0.0f);
            phyThrust->thrustLocal = vec2(0.0f);

            // LG_D("PhyThrust update for entity: {} PhyThrust: {}", entity,
            // *phyThrust);
        }
    }};

const System sysPhysics = {
    "sysPhysics",
    [](entt::entity entity,
       const ecs::EntityId& entityId,
       float dt,
       std::shared_ptr<PtrHandle> ptrHandle)
    {
        auto reg = ptrHandle->registry;
        auto* sectorId = reg->try_get<ecs::SectorId>(entity);
        auto* transform = reg->try_get<Transform>(entity);
        auto* physicsBody = reg->try_get<PhysicsBody>(entity);
        if (transform && physicsBody)
        {
            physicsBody->vel += physicsBody->acc * dt;
            physicsBody->rotVel += physicsBody->rotAcc * dt;
            transform->pos += physicsBody->vel * dt;
            transform->rot += physicsBody->rotVel * dt;
            if (transform->rot < 0.0f)
            {
                transform->rot += 2.0f * M_PIf;
            }
            else if (transform->rot >= 2.0f * M_PIf)
            {
                transform->rot -= 2.0f * M_PIf;
            }

            // Check for sector switch
            ptrHandle->world->checkSectorSwitchAfterMove(
                entityId, entity, sectorId, transform, ptrHandle);

            // Reset acceleration after game update
            physicsBody->acc = {0, 0};
            physicsBody->rotAcc = 0;

            // LG_D("PhysicsBody update for entity: {} PhysicsBody: {}", entity,
            // *physicsBody); LG_D("Transform update for entity: {} Transform:
            // {}", entity, *transform);
        }
    }};

}  // namespace ecs

#endif