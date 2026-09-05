#include "aabb-tree.hpp"
#include "comp-collavoid.hpp"
#include "comp-ident.hpp"
#include "comp-phy.hpp"
#include "config-manager.hpp"
#include "entt/entity/fwd.hpp"
#include "glm/geometric.hpp"
#include "logging.hpp"
#include "magic_enum/magic_enum.hpp"
#include "pool-objects.hpp"
#include "ptr-handle.hpp"
#include "sector.hpp"
#include <engine.hpp>
#include <sys-collavoid.hpp>

#define CFG_PATH_BP "engine", "physics", "avoidance", "broadphase"
#define CFG_PATH_EXTR "engine", "physics", "avoidance", "extrapolation"

namespace ecs
{

static float cfg_swp_cone;
static float cfg_swp_overlap;
static float cfg_swp_t0;
static float cfg_swp_time_horizon;
static float cfg_frameskip;
static float cfg_extr_step;

void initCollAvoid(const cfg::ConfigManager& config)
{
    cfg_swp_cone = CFG_FLOAT(config, 50.0f, CFG_PATH_BP, "swp-cone");
    cfg_swp_t0 = CFG_FLOAT(config, 0.1f, CFG_PATH_BP, "swp-t0");
    cfg_swp_overlap = CFG_FLOAT(config, 1.0f, CFG_PATH_BP, "swp-overlap");
    cfg_swp_time_horizon =
        CFG_FLOAT(config, 5.0f, CFG_PATH_BP, "swp-time-horizon");
    cfg_frameskip = CFG_FLOAT(config, 1.5f, CFG_PATH_BP, "frameskip");
    cfg_extr_step = CFG_FLOAT(config, 0.5f, CFG_PATH_EXTR, "step-size");
}

static void collAvoidBroadphaseSweep(PtrHandle* ptrHandle,
                                     world::Sector* sector,
                                     const EntityId entityId,
                                     entt::entity entity,
                                     const Transform& tr,
                                     const con::AABB& aabb,
                                     const PhysicsBody& phy)
{
    vector<vec3> quads;
    const float c =
        std::max(aabb.upper.x - aabb.lower.x, aabb.upper.y - aabb.lower.y);
    float spd = glm::length(phy.vel);
    auto reg = sector->getRegistry()->getRegistry();
    if (spd > 0.5f)
    {
        float t_i = cfg_swp_t0;
        vec2 p_i;
        int i = 0;
        do
        {
            i++;
            // Current quad
            p_i = t_i * phy.vel;
            const float r_i = cfg_swp_cone * t_i + c;
            const vec2 secPos = tr.pos + p_i;
            quads.push_back({secPos.x, secPos.y, r_i * 2.0f});
            // Next quad
            t_i = (glm::length(p_i) + cfg_swp_overlap * c)
                  / (spd - cfg_swp_overlap * cfg_swp_cone);
            const vec2 halfSize(r_i, r_i);
            const con::AABB aabb = {.lower = secPos - halfSize,
                                    .upper = secPos + halfSize};
            sector->queryBroadphase(
                aabb,
                [entity, reg, sector](const world::BpUserData& data)
                {
                    if (data.type == world::BpUserType::Ecs)
                    {
                        auto entityOther = data.data.ent;
                        if (entityOther == entity)
                        {
                            return;
                        }
                        entt::entity lo = entity;
                        entt::entity hi = entityOther;
                        if (hi < lo)
                        {
                            std::swap(lo, hi);
                        }
                        sector->collAvoidanceBroadphase.push_back({lo, hi});
                    }
                });
        } while (t_i > 0 && t_i < cfg_swp_time_horizon && i < 10);
    }
    ptrHandle->engine->debugSendCollAvoidInfo(entityId, quads);
}

void sysCollAvoidImpl(world::Sector* sector, float dt, PtrHandle* ptrHandle)
{
    auto* reg = sector->getRegistry()->getRegistry();
    sector->collAvoidanceBroadphase.clear();
    sector->collAvoidPool.clear();
    reg->view<EntityId, CollAvoid, Transform, PhysicsBody, Broadphase>().each(
        [ptrHandle, sector](auto entity,
                            EntityId& entityId,
                            CollAvoid& collAvoid,
                            Transform& tr,
                            PhysicsBody& phy,
                            Broadphase& bp)
        {
            if (collAvoid.active)
            {
                if (ptrHandle->frameCnt < collAvoid.nextRunFrame)
                {
                    return;
                }
                // todo: collision avoidance scan
                collAvoidBroadphaseSweep(
                    ptrHandle, sector, entityId, entity, tr, bp.fatAABB, phy);
                collAvoid.nextRunFrame = ptrHandle->frameCnt + cfg_frameskip;
            }
        });
    // Deduplicate
    std::sort(sector->collAvoidanceBroadphase.begin(),
              sector->collAvoidanceBroadphase.end());
    sector->collAvoidanceBroadphase.erase(
        std::unique(sector->collAvoidanceBroadphase.begin(),
                    sector->collAvoidanceBroadphase.end()),
        sector->collAvoidanceBroadphase.end());

    for (auto& bpPair : sector->collAvoidanceBroadphase)
    {
        auto ent1 = bpPair.first;
        auto ent2 = bpPair.second;
        if (!reg->valid(ent1) || !reg->valid(ent2))
        {
            continue;
        }
        // Get Transform and physics of the two actors and interpolate
        auto tr1 = reg->get<Transform>(ent1);
        auto tr2 = reg->get<Transform>(ent2);
        auto phy1 = reg->get<PhysicsBody>(ent1);
        auto phy2 = reg->get<PhysicsBody>(ent2);
        auto bp1 = reg->get<Broadphase>(ent1);
        auto bp2 = reg->get<Broadphase>(ent2);
        // Time interpolation and stepwise AABB comparison
        for (float t = 0.0f; t < cfg_swp_time_horizon; t += cfg_extr_step)
        {
            const vec2 d1 = phy1.vel * t;
            const vec2 d2 = phy2.vel * t;
            const vec2 epos1 = tr1.pos + d1;
            const vec2 epos2 = tr2.pos + d2;
            const con::AABB aabb1 = bp1.fatAABB.move(d1);
            const con::AABB aabb2 = bp2.fatAABB.move(d2);
            if (aabb1.overlaps(aabb2))
            {
                //LG_D("Imminent collision {:.2},{:.2}", epos1.x, epos1.y);
                auto id1 = reg->get<EntityId>(ent1);
                auto id2 = reg->get<EntityId>(ent2);
                sector->collAvoidPool.spawnObject(opool::DbgCollAvoid{
                    .id1 = id1, .id2 = id2, .intersect = aabb1.center()});
                break;
            }
        }
    }
}

}  // namespace ecs
