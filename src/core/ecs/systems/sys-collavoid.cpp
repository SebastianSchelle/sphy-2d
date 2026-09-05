#include "aabb-tree.hpp"
#include "comp-collavoid.hpp"
#include "comp-ident.hpp"
#include "comp-phy.hpp"
#include "config-manager.hpp"
#include "entt/entity/fwd.hpp"
#include "glm/geometric.hpp"
#include "logging.hpp"
#include "magic_enum/magic_enum.hpp"
#include "ptr-handle.hpp"
#include "sector.hpp"
#include <engine.hpp>
#include <sys-collavoid.hpp>

#define CFG_PATH_BP "engine", "physics", "avoidance", "broadphase"

namespace ecs
{

static float cfg_swp_cone;
static float cfg_swp_overlap;
static float cfg_swp_t0;
static float cfg_swp_time_horizon;
static float cfg_frameskip;

void initCollAvoid(const cfg::ConfigManager& config)
{
    cfg_swp_cone = CFG_FLOAT(config, 50.0f, CFG_PATH_BP, "swp-cone");
    cfg_swp_t0 = CFG_FLOAT(config, 0.1f, CFG_PATH_BP, "swp-t0");
    cfg_swp_overlap = CFG_FLOAT(config, 1.0f, CFG_PATH_BP, "swp-overlap");
    cfg_swp_time_horizon =
        CFG_FLOAT(config, 5.0f, CFG_PATH_BP, "swp-time-horizon");
    cfg_frameskip = CFG_FLOAT(config, 1.5f, CFG_PATH_BP, "frameskip");
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
            const con::AABB aabb = {.lower = tr.pos - halfSize,
                                    .upper = tr.pos + halfSize};
            sector->queryBroadphase(aabb, [entity](const world::BpUserData &data){
                if(data.type == world::BpUserType::Ecs)
                {
                    auto entOther = data.data.ent;
                    if(entOther == entity)
                    {
                        return;
                    }
                    LG_D("avoid {}", magic_enum::enum_name(data.type));
                }
            });
        } while (t_i > 0 && t_i < cfg_swp_time_horizon && i < 10);
    }

    ptrHandle->engine->debugSendCollAvoidInfo(entityId, quads);
}

void sysCollAvoidImpl(world::Sector* sector, float dt, PtrHandle* ptrHandle)
{
    auto* reg = sector->getRegistry()->getRegistry();
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
}

}  // namespace ecs
