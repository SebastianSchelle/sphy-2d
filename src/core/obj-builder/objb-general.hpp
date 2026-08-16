#ifndef OBJB_GENERAL_HPP
#define OBJB_GENERAL_HPP

#include "comp-ai.hpp"
#include "comp-gfx.hpp"
#include "comp-ident.hpp"
#include "comp-lifetime.hpp"
#include "comp-storage.hpp"
#include "entt/entity/entity.hpp"
#include "entt/entity/fwd.hpp"
#include "lib-textures.hpp"
#include "ship-def.hpp"
#include "task-system.hpp"
#include <comp-phy.hpp>
#include <comp-tag.hpp>
#include <lib-collider.hpp>
#include <mod-manager.hpp>
#include <ptr-handle.hpp>
#include <sector-registry.hpp>
#include <variant>

#define OBJB_GUARD(fun, ...)                                                   \
    if (!(fun))                                                                \
    {                                                                          \
        LG_E(__VA_ARGS__);                                                     \
        return false;                                                          \
    }

namespace objb
{

struct Transform
{
    static bool build(ecs::PtrHandle* ptrHandle,
                      ecs::SpawnCallbackParams& params,
                      ecs::Transform transform = {ecs::Transform::ZERO()})
    {
        params.reg.emplace_or_replace<ecs::Transform>(params.entity, transform);
        params.reg.emplace_or_replace<ecs::TransformCache>(
            params.entity,
            ecs::TransformCache{.c = cosf(transform.rot),
                                .s = sinf(transform.rot)});
        return true;
    }

    static void
    position(entt::registry* reg, entt::entity entity, vec2 pos, float rot)
    {
        auto* transform = reg->try_get<ecs::Transform>(entity);
        auto* transCache = reg->try_get<ecs::TransformCache>(entity);
        if (transform && transCache)
        {
            transform->pos = pos;
            transform->rot = rot;
            transCache->c = cosf(rot);
            transCache->s = sinf(rot);
        }
    }
};

struct AnchorFixed
{
    static bool build(ecs::PtrHandle* ptrHandle,
                      ecs::SpawnCallbackParams& params,
                      ecs::AnchorFixed anchor)
    {
        params.reg.emplace_or_replace<ecs::AnchorFixed>(params.entity, anchor);
        return true;
    }
};

struct Ai
{
    static bool build(ecs::PtrHandle* ptrHandle,
                      ecs::SpawnCallbackParams& params,
                      const ai::taskdata::TaskData& defaultTask)
    {
        auto& aiComp = params.reg.get_or_emplace<ecs::Ai>(params.entity);
        if (ai::TaskStackHandle(aiComp.stackHandle).isValid())
        {
            auto* taskStack =
                params.taskSystem.getTaskStack(aiComp.stackHandle);
            if (!taskStack)
            {
                LG_W("Task stack not found for entity, creating new stack");
                aiComp.stackHandle = GenericHandle32::Invalid();
            }
            else
            {
                taskStack->setDefaultTask(defaultTask);
                return true;
            }
        }
        auto stackHandle = params.taskSystem.createTaskStack(defaultTask);
        aiComp.stackHandle = stackHandle.toGenericHandle();
        return true;
    }
};

struct Selectable
{
    static bool build(ecs::PtrHandle* ptrHandle,
                      ecs::SpawnCallbackParams& params,
                      ecs::Transform transform = {ecs::Transform::ZERO()})
    {
        params.reg.emplace_or_replace<ecs::tag::Selectable>(params.entity);
        return true;
    }
};

struct Broadphase
{
    static bool build(ecs::PtrHandle* ptrHandle,
                      ecs::SpawnCallbackParams& params)
    {
        params.reg.emplace_or_replace<ecs::Broadphase>(params.entity);
        return true;
    }
};

struct Collider
{
    static bool build(ecs::PtrHandle* ptrHandle,
                      ecs::SpawnCallbackParams& params,
                      const gobj::ColliderHandle& collHandle,
                      ecs::CollisionLayer colliderType,
                      ecs::EntityId exceptEntId = ecs::EntityId::Invalid())
    {
        gobj::Collider* coll =
            ptrHandle->modManager->getColliderLib().getItem(collHandle);
        OBJB_GUARD(coll,
                   "Could not find collider entry for {}",
                   collHandle.toGenericHandle())
        OBJB_GUARD(Broadphase::build(ptrHandle, params), "")
        params.reg.emplace_or_replace<ecs::Collider>(
            params.entity,
            ecs::Collider{.colliderHandle = collHandle.toGenericHandle(),
                          .exceptEntity = exceptEntId,
                          .colliderType = colliderType});
        return true;
    }
};

struct Textures
{
    static bool build(ecs::PtrHandle* ptrHandle,
                      ecs::SpawnCallbackParams& params,
                      const gobj::TexturesHandle& texHandle)
    {
        gobj::Textures* tex =
            ptrHandle->modManager->getTexturesLib().getItem(texHandle);
        OBJB_GUARD(tex,
                   "Could not find textures entry for {}",
                   texHandle.toGenericHandle())
        params.reg.emplace_or_replace<ecs::Textures>(
            params.entity,
            ecs::Textures{.texturesHandle = texHandle.toGenericHandle()});
        return true;
    }
};

struct Storage
{
    static bool build(ecs::PtrHandle* ptrHandle,
                      ecs::SpawnCallbackParams& params,
                      const ecs::Storage& storage)
    {
        params.reg.emplace_or_replace<ecs::Storage>(params.entity, storage);
        return true;
    }
};

struct Lifetime
{
    static bool build(ecs::PtrHandle* ptrHandle,
                      ecs::SpawnCallbackParams& params,
                      float lifetime)
    {
        params.reg.emplace_or_replace<ecs::Lifetime>(
            params.entity, ecs::Lifetime{.lifetime = lifetime});
        return true;
    }
};

struct Physics
{
    static bool build(ecs::PtrHandle* ptrHandle,
                      ecs::SpawnCallbackParams& params,
                      const ecs::PhysicsBody& phyBody)
    {
        OBJB_GUARD(Transform::build(ptrHandle, params), "")
        params.reg.emplace_or_replace<ecs::PhysicsBody>(params.entity, phyBody);
        return true;
    }

    static void
    naturalRot(entt::registry* reg, entt::entity entity, float naturalRot)
    {
        auto* phy = reg->try_get<ecs::PhysicsBody>(entity);
        if (phy)
        {
            phy->naturalRotation = naturalRot;
        }
    }

    static void velocity(entt::registry* reg, entt::entity entity, vec2 vel)
    {
        auto* phy = reg->try_get<ecs::PhysicsBody>(entity);
        if (phy)
        {
            phy->vel = vel;
        }
    }
};

struct ThrusterMoveCtrl
{
    static bool build(ecs::PtrHandle* ptrHandle,
                      ecs::SpawnCallbackParams& params,
                      const ecs::PhyThrust& phyThrust,
                      const ecs::MoveCtrl& moveCtrl)
    {
        params.reg.emplace_or_replace<ecs::PhyThrust>(params.entity, phyThrust);
        params.reg.emplace_or_replace<ecs::MoveCtrl>(params.entity, moveCtrl);
        return true;
    }
};

struct IconStation
{
};
struct IconShipHull
{
    def::ShipClass sClass;
};
typedef std::variant<IconStation, IconShipHull> IconSelector;

struct MapIcon
{
    static bool build(ecs::PtrHandle* ptrHandle,
                      ecs::SpawnCallbackParams& params,
                      IconSelector iconSelector)
    {
        struct IconVisitor
        {
            ecs::PtrHandle* ptrHandle;
            gobj::MapIconHandle mapIconHandle = gobj::MapIconHandle::Invalid();

            void operator()(const IconStation&)
            {
                mapIconHandle =
                    ptrHandle->modManager->getMapIconLib().getHandle("station");
            }

            void operator()(const IconShipHull& icon)
            {
                switch (icon.sClass)
                {
                    case def::ShipClass::Spark:
                        mapIconHandle =
                            ptrHandle->modManager->getMapIconLib().getHandle(
                                "spark");
                        break;
                    case def::ShipClass::Echo:
                        mapIconHandle =
                            ptrHandle->modManager->getMapIconLib().getHandle(
                                "echo");
                        break;
                    default:
                        mapIconHandle =
                            ptrHandle->modManager->getMapIconLib().getHandle(
                                "echo");
                        break;
                }
            }
        };

        IconVisitor visitor{ptrHandle};
        std::visit(visitor, iconSelector);

        OBJB_GUARD(visitor.mapIconHandle.isValid(),
                   "MapIcon: Could not resolve map icon handle")
        params.reg.emplace_or_replace<ecs::MapIcon>(
            params.entity,
            ecs::MapIcon{.mapIconHandle =
                             visitor.mapIconHandle.toGenericHandle()});
        return true;
    }
};

}  // namespace objb

#endif