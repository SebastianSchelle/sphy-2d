#include "comp-phy.hpp"
#include "entt/entity/fwd.hpp"
#include "free-vector.hpp"
#include "lib-projectile.hpp"
#include "sector.hpp"
#include "std-inc.hpp"
#include "sys-phy.hpp"
#include <lib-collider.hpp>
#include <mod-manager.hpp>
#include <sys-specsys.hpp>
#include <world.hpp>

namespace ecs
{

static inline void projColliderAction(PtrHandle* ptrHandle,
                                      world::Sector* sector,
                                      CollisionLayer collLayer,
                                      const entt::entity other,
                                      const gobj::Projectile& projData,
                                      const vec2& collPos)
{
    auto reg = sector->getRegistry()->getRegistry();
    switch (collLayer)
    {
        case CollisionLayer::Asteroid:
        {
            auto* asteroid = reg->try_get<Asteroid>(other);
            if (!asteroid)
            {
                return;
            }
            float dmg = (projData.damageType == def::DamageType::Mining
                             ? projData.dmg
                             : projData.dmg * 0.001f)
                        * ptrHandle->miningRate;
            damageAndMine(
                *asteroid, ptrHandle, sector, projData.dmg, other, collPos);
        }
        default:
            break;
    }
}

void sysProjPhysicsImpl(world::Sector* sector, float dt, PtrHandle* ptrHandle)
{
    sector->foreachProj(
        [ptrHandle, dt, sector](opool::Projectile& projectile,
                                opool::ProjectileHandle handle)
        {
            // LIFETIME
            projectile.lifetime += dt;
            if (projectile.lifetime > projectile.lifetimeMax)
            {
                return con::FreeVecForeachRet::DESTROY;
            }

            // MOVEMENT
            auto& ws = ptrHandle->world->getWorldShape();
            auto& trans = projectile.transform;
            trans.pos += projectile.vel * dt;

            const float ws2 = ws.sectorSize / 2.0f;
            if (trans.pos.x < -ws2 || trans.pos.x > ws2 || trans.pos.y < -ws2
                || trans.pos.y > ws2)
            {
                return con::FreeVecForeachRet::DESTROY;
            }

            // COLLISION
            auto slot =
                ptrHandle->registryMapping->getEntity(projectile.collExcept);
            auto ret = con::FreeVecForeachRet::OK;
            sector->queryBroadphasePoint(
                trans.pos,
                [slot, sector, &ret, ptrHandle, &trans, &projectile](
                    const world::BpUserData& data)
                {
                    if (data.type == world::BpUserType::Ecs)
                    {
                        auto other = data.data.ent;
                        if (slot && other == slot->entity
                            && slot->sectorId == sector->getId())
                        {
                            return;
                        }
                        auto reg = sector->getRegistry()->getRegistry();
                        auto coll = reg->try_get<ecs::Collider>(other);
                        auto tr = reg->try_get<ecs::Transform>(other);
                        auto trc = reg->try_get<ecs::TransformCache>(other);

                        if (coll && tr && trc)
                        {
                            auto collItem =
                                ptrHandle->modManager->getColliderLib().getItem(
                                    coll->colliderHandle);

                            const auto v1 = &collItem->vertices;
                            const size_t n1 = v1->size();
                            thread_local std::vector<vec2> w1;
                            w1.resize(v1->size());
                            for (size_t i = 0; i < n1; ++i)
                            {
                                const vec2& v = (*v1)[i];
                                w1[i].x =
                                    trc->c * v.x - trc->s * v.y + tr->pos.x;
                                w1[i].y =
                                    trc->s * v.x + trc->c * v.y + tr->pos.y;
                            }
                            if (sat2d::pointInConvex(trans.pos, w1))
                            {
                                auto projData =
                                    ptrHandle->modManager->getProjectileLib()
                                        .getItem(projectile.proj);
                                if (projData)
                                {
                                    projColliderAction(
                                        ptrHandle,
                                        sector,
                                        coll->colliderType,
                                        other,
                                        *projData,
                                        projectile.transform.pos);
                                }
                                ret = con::FreeVecForeachRet::DESTROY;
                            }
                        }
                    }
                });

            return ret;
        });
}

void sysItemPhysicsImpl(world::Sector* sector, float dt, PtrHandle* ptrHandle)
{
    sector->foreachItem(
        [ptrHandle, dt, sector](opool::Item& item, opool::ItemHandle handle)
        {
            // LIFETIME
            item.lifetime += dt;
            if (item.lifetime > item.lifetimeMax)
            {
                return con::FreeVecForeachRet::DESTROY;
            }

            // MOVEMENT
            if (fabs(item.vel.x) > 0.01f || fabs(item.vel.y) > 0.01f)
            {
                auto& ws = ptrHandle->world->getWorldShape();
                auto& trans = item.transform;
                const vec2 posOld = trans.pos;
                trans.pos += item.vel * dt;
                item.vel -= glm::normalize(item.vel) * 5.0f * dt;

                // Stop pseudo physics if sector border
                const float ws2 = ws.sectorSize / 2.0f;
                if (trans.pos.x < -ws2 || trans.pos.x > ws2
                    || trans.pos.y < -ws2 || trans.pos.y > ws2)
                {
                    trans.pos = posOld;
                    item.vel = vec2(0.0f, 0.0f);
                }
                else
                {
                    sector->itemUpdateBroadphase(ptrHandle, handle, item);
                }

                // Stop if something is hit
                // auto slot =
                //     ptrHandle->registryMapping->getEntity(item.collExcept);
                // sector->queryBroadphasePoint(
                //     trans.pos,
                //     [slot, sector, ptrHandle, &trans, &item](
                //         const world::BpUserData& data)
                //     {
                //         if (data.type == world::BpUserType::Ecs)
                //         {
                //             // todo: For now don't care about ecs collision when moving
                //             // auto other = data.data.ent;
                //             // if (slot && other == slot->entity)
                //             // {
                //             //     return;
                //             // }

                //             // auto reg = sector->getRegistry()->getRegistry();
                //             // auto coll = reg->try_get<ecs::Collider>(other);
                //             // auto tr = reg->try_get<ecs::Transform>(other);
                //             // auto trc = reg->try_get<ecs::TransformCache>(other);
                //             // if (coll && tr && trc)
                //             // {
                //             //     auto collItem =
                //             //         ptrHandle->modManager->getColliderLib()
                //             //             .getItem(coll->colliderHandle);

                //             //     const auto v1 = &collItem->vertices;
                //             //     const size_t n1 = v1->size();
                //             //     thread_local std::vector<vec2> w1;
                //             //     w1.resize(v1->size());
                //             //     for (size_t i = 0; i < n1; ++i)
                //             //     {
                //             //         const vec2& v = (*v1)[i];
                //             //         w1[i].x =
                //             //             trc->c * v.x - trc->s * v.y + tr->pos.x;
                //             //         w1[i].y =
                //             //             trc->s * v.x + trc->c * v.y + tr->pos.y;
                //             //     }
                //             //     if (sat2d::pointInConvex(trans.pos, w1))
                //             //     {
                //             //         item.vel = vec2(0.0f, 0.0f);
                //             //     }
                //             // }
                //         }
                //         else if (data.type == world::BpUserType::Item)
                //         {
                //             // when near other item stop
                //             item.vel = vec2(0.0f, 0.0f);
                //         }
                //     });
            }
            return con::FreeVecForeachRet::OK;
        });
}

}  // namespace ecs
