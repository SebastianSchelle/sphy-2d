#ifndef OBJB_ITEM_HPP
#define OBJB_ITEM_HPP

#include "comp-ident.hpp"
#include "comp-phy.hpp"
#include "comp-storage.hpp"
#include "entt/entity/fwd.hpp"
#include "lib-asteroid.hpp"
#include <comp-struct.hpp>
#include <mod-manager.hpp>
#include <objb-general.hpp>

namespace objb
{

struct Item
{
    static bool build(ecs::PtrHandle* ptrHandle,
                      ecs::SpawnCallbackParams& params,
                      const gobj::ItemHandle& itemHandle,
                      ecs::EntityId collExcept = ecs::EntityId::Invalid())
    {
        OBJB_GUARD(Selectable::build(ptrHandle, params), "")
        gobj::Item* item =
            ptrHandle->modManager->getItemLib().getItem(itemHandle);
        OBJB_GUARD(item,
                   "Item: Could not find item entry for {}",
                   itemHandle.toGenericHandle())
        params.reg.emplace_or_replace<ecs::Item>(
            params.entity,
            ecs::Item{.itemHandle = itemHandle.toGenericHandle(),
                      .quantity = 1.0f});
        OBJB_GUARD(SimpleTexture::build(ptrHandle, params, item->worldTexture),
                   "Item: Failed to build world textures")
        auto colliderHandle =
            ptrHandle->modManager->getColliderLib().getHandle("Item");
        OBJB_GUARD(
            Collider::build(
                ptrHandle, params, colliderHandle, ecs::CollisionLayer::Item, collExcept),
            "Item: Failed to build collider")
        OBJB_GUARD(Physics::build(ptrHandle,
                                  params,
                                  ecs::PhysicsBody{.mass = 100.0f,
                                                   .vel = vec2(0.0f, 0.0f),
                                                   .acc = vec2(0.0f, 0.0f),
                                                   .inertia = 1.0f,
                                                   .rotVel = 0.0f,
                                                   .rotAcc = 0.0f}),
                   "")
        OBJB_GUARD(Lifetime::build(ptrHandle, params, ptrHandle->itemLifetime),
                   "")
        return true;
    }

    static void
    quantity(entt::registry* reg, entt::entity entity, float quantity)
    {
        auto* item = reg->try_get<ecs::Item>(entity);
        if (item)
        {
            item->quantity = quantity;
        }
    }
};

}  // namespace objb

#endif