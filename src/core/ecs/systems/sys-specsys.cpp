#include "aabb-tree.hpp"
#include "comp-phy.hpp"
#include "comp-storage.hpp"
#include "entt/entity/fwd.hpp"
#include "free-vector.hpp"
#include "lib-projectile.hpp"
#include "pool-objects.hpp"
#include "sector.hpp"
#include "std-inc.hpp"
#include "sys-phy.hpp"
#include "turret-def.hpp"
#include <cmath>
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
            damageAndMine(*asteroid, ptrHandle, sector, dmg, other, collPos);
        }
        default:
            break;
    }
}

static inline void beamColliderAction(PtrHandle* ptrHandle,
                                      world::Sector* sector,
                                      CollisionLayer collLayer,
                                      const entt::entity other,
                                      const gobj::Beam& beamData,
                                      const vec2& collPos,
                                      float dt)
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
            float dps = (beamData.damageType == def::DamageType::Mining
                             ? beamData.dps
                             : beamData.dps * 0.001f)
                        * ptrHandle->miningRate;
            damageAndMine(
                *asteroid, ptrHandle, sector, dps * dt, other, collPos);
        }
        default:
            break;
    }
}

void sysProjPhysicsImpl(world::Sector* sector, float dt, PtrHandle* ptrHandle)
{
    sector->foreachOpool<opool::Projectile>(
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
                        auto coll = reg->get<ecs::Collider>(other);
                        auto collItem =
                            ptrHandle->modManager->getColliderLib().getItem(
                                coll.colliderHandle);
                        if (!collItem)
                        {
                            return;
                        }
                        auto tr = reg->get<ecs::Transform>(other);
                        auto trc = reg->get<ecs::TransformCache>(other);
                        thread_local std::vector<vec2> w1;
                        sat2d::translateVertices(
                            collItem->vertices, w1, tr.pos, trc.c, trc.s);
                        if (sat2d::pointInConvex(trans.pos, w1))
                        {
                            auto projData =
                                ptrHandle->modManager->getProjectileLib()
                                    .getItem(projectile.proj);
                            if (projData)
                            {
                                projColliderAction(ptrHandle,
                                                   sector,
                                                   coll.colliderType,
                                                   other,
                                                   *projData,
                                                   projectile.transform.pos);
                            }
                            ret = con::FreeVecForeachRet::DESTROY;
                        }
                    }
                });

            return ret;
        });
}


struct BeamHit
{
    float hitT = INFINITY;
    vec2 hitPoint;
    world::BpUserData data;
    ecs::CollisionLayer collLayer;
};

void sysBeamPhysicsImpl(world::Sector* sector, float dt, PtrHandle* ptrHandle)
{
    sector->foreachOpool<opool::Beam>(
        [ptrHandle, dt, sector](opool::Beam& beam, opool::BeamHandle handle)
        {
            auto reg = sector->getRegistry()->getRegistry();
            auto beamData =
                ptrHandle->modManager->getBeamLib().getItem(beam.beam);
            if (!beamData)
            {
                return con::FreeVecForeachRet::DESTROY;
            }
            if(beamData->damageType == def::DamageType::Collector)
            {
                return con::FreeVecForeachRet::OK;
            }
            const vec2 pos = beam.origin.pos;
            const float rot = beam.origin.rot;
            const float s = sinf(rot);
            const float c = cosf(rot);
            const vec2 pos2 =
                pos
                + smath::rotateVec2(beamData->range * vec2(0.0f, 1.0f), s, c);
            const vec2 aa =
                vec2(std::min(pos.x, pos2.x), std::min(pos.y, pos2.y));
            const vec2 bb =
                vec2(std::max(pos.x, pos2.x), std::max(pos.y, pos2.y));
            const con::AABB aabb = {.lower = aa, .upper = bb};
            const auto slot =
                ptrHandle->registryMapping->getEntity(beam.collExcept);

            BeamHit hit;
            sector->queryBroadphase(
                aabb,
                [&beam, &beamData, slot, reg, ptrHandle, s, c, &hit](
                    const world::BpUserData& data)
                {
                    if (data.type == world::BpUserType::Ecs)
                    {
                        auto other = data.data.ent;
                        if (slot && other == slot->entity)
                        {
                            return;
                        }
                        auto coll = reg->get<ecs::Collider>(other);
                        auto collItem =
                            ptrHandle->modManager->getColliderLib().getItem(
                                coll.colliderHandle);
                        if (!collItem)
                        {
                            return;
                        }
                        auto tr = reg->get<ecs::Transform>(other);
                        auto trc = reg->get<ecs::TransformCache>(other);
                        thread_local std::vector<vec2> w1;
                        sat2d::translateVertices(
                            collItem->vertices, w1, tr.pos, trc.c, trc.s);
                        const vec2 dir =
                            smath::rotateVec2(vec2(0.0f, 1.0f), s, c);
                        vec2 hitPoint;
                        float hitT;
                        if (sat2d::rayVsConvex(beam.origin.pos,
                                               dir,
                                               beamData->range,
                                               w1,
                                               hitT,
                                               hitPoint))
                        {
                            if (hitT < hit.hitT)
                            {
                                hit.hitT = hitT;
                                hit.hitPoint = hitPoint;
                                hit.data = data;
                                hit.collLayer = coll.colliderType;
                            }
                        }
                    }
                });
            if (hit.hitT != INFINITY)
            {
                beam.point2 = hit.hitPoint;
                const auto& data = hit.data;
                if (data.type == world::BpUserType::Ecs)
                {
                    beamColliderAction(ptrHandle,
                                       sector,
                                       hit.collLayer,
                                       data.data.ent,
                                       *beamData,
                                       hit.hitPoint,
                                       dt);
                }
            }
            else
            {
                beam.point2 = beam.origin.pos
                              + beamData->range
                                    * smath::rotateVec2(vec2(0.0f, 1.0f), s, c);
            }
            return con::FreeVecForeachRet::OK;
        });
}

void sysItemPhysicsImpl(world::Sector* sector, float dt, PtrHandle* ptrHandle)
{
    sector->foreachOpool<opool::Item>(
        [ptrHandle, dt, sector](opool::Item& item, opool::ItemHandle handle)
        {
            con::FreeVecForeachRet ret = con::FreeVecForeachRet::OK;

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

                auto slot =
                    ptrHandle->registryMapping->getEntity(item.collExcept);
                sector->queryBroadphasePoint(
                    trans.pos,
                    [slot, sector, ptrHandle, &trans, &item, &ret](
                        const world::BpUserData& data)
                    {
                        if (data.type == world::BpUserType::Ecs)
                        {
                            // todo: For now don't care about ecs collision
                            // when moving
                            auto other = data.data.ent;
                            if (slot && other == slot->entity)
                            {
                                return;
                            }

                            auto reg = sector->getRegistry()->getRegistry();
                            auto coll = reg->get<ecs::Collider>(other);
                            auto tr = reg->get<ecs::Transform>(other);
                            auto trc = reg->get<ecs::TransformCache>(other);
                            auto collItem =
                                ptrHandle->modManager->getColliderLib().getItem(
                                    coll.colliderHandle);
                            if (!collItem)
                            {
                                return;
                            }
                            const auto v1 = &collItem->vertices;
                            const size_t n1 = v1->size();
                            thread_local std::vector<vec2> w1;
                            w1.resize(v1->size());
                            for (size_t i = 0; i < n1; ++i)
                            {
                                const vec2& v = (*v1)[i];
                                w1[i].x = trc.c * v.x - trc.s * v.y + tr.pos.x;
                                w1[i].y = trc.s * v.x + trc.c * v.y + tr.pos.y;
                            }
                            if (sat2d::pointInConvex(trans.pos, w1))
                            {
                                if (coll.colliderType
                                    == ecs::CollisionLayer::Ship)
                                {
                                    auto* storage =
                                        reg->try_get<ecs::Storage>(other);
                                    auto* itemData =
                                        ptrHandle->modManager->getItemLib()
                                            .getItem(item.item);
                                    if (storage && itemData)
                                    {
                                        uint32_t amountAdded =
                                            storage->tryAddItem(item.item,
                                                                *itemData,
                                                                item.quantity);
                                        item.quantity -= amountAdded;
                                        if (item.quantity <= 0)
                                        {
                                            ret =
                                                con::FreeVecForeachRet::DESTROY;
                                        }
                                    }
                                }
                            }
                        }
                        else if (data.type == world::BpUserType::Item)
                        {
                            // when near other item stop
                            // item.vel = vec2(0.0f, 0.0f);
                        }
                    });
            }
            return ret;
        });
}

}  // namespace ecs
