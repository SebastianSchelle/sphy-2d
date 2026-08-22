#include "comp-phy.hpp"
#include "entt/entity/fwd.hpp"
#include "free-vector.hpp"
#include "lib-projectile.hpp"
#include "logging.hpp"
#include "sector.hpp"
#include "std-inc.hpp"
#include "sys-phy.hpp"
#include <lib-collider.hpp>
#include <mod-manager.hpp>
#include <projectile.hpp>
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
        [ptrHandle, dt, sector](specsys::Projectile& projectile,
                                specsys::ProjectileHandle handle)
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

}  // namespace ecs
