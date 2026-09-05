#include "aabb-tree.hpp"
#include "comp-collavoid.hpp"
#include "comp-ident.hpp"
#include "comp-phy.hpp"
#include "config-manager.hpp"
#include "glm/geometric.hpp"
#include "ptr-handle.hpp"
#include <engine.hpp>
#include <sys-collavoid.hpp>

#define CFG_PATH_BP "engine", "physics", "avoidance", "broadphase"

namespace ecs
{

#define MAX_SWEEP_STEPS 64
static float cfg_rb_max;
static float cfg_v_max;
static float cfg_sweep_d1;
static float cfg_sweep_time_horizon;
static float cfg_sweep_d_mul;
static float sweep_steps[MAX_SWEEP_STEPS];

static float cfg_frameskip;

void initCollAvoid(const cfg::ConfigManager& config)
{
    cfg_rb_max = CFG_FLOAT(config, 50.0f, CFG_PATH_BP, "rb_max");
    cfg_v_max = CFG_FLOAT(config, 100.0f, CFG_PATH_BP, "v_max");
    cfg_sweep_d1 = CFG_FLOAT(config, 0.5f, CFG_PATH_BP, "sweep_d1");
    cfg_sweep_time_horizon =
        CFG_FLOAT(config, 5.0f, CFG_PATH_BP, "sweep_time_horizon");
    cfg_sweep_d_mul = CFG_FLOAT(config, 1.5f, CFG_PATH_BP, "sweep_d_mul");
    cfg_frameskip = CFG_FLOAT(config, 1.5f, CFG_PATH_BP, "frameskip");

    sweep_steps[0] = cfg_sweep_d1;
    for (int i = 1; i < MAX_SWEEP_STEPS; ++i)
    {
        sweep_steps[i] = sweep_steps[i - 1] * cfg_sweep_d_mul;
    }
}

static void collAvoidBroadphaseSweep(PtrHandle* ptrHandle,
                                     const EntityId entityId,
                                     const Transform& tr,
                                     const con::AABB& aabb,
                                     const PhysicsBody& phy)
{
    const float ra =
        std::max(aabb.upper.x - aabb.lower.x, aabb.upper.y - aabb.lower.y);
    const float rc = ra + cfg_rb_max;
    float spd = glm::length(phy.vel);
    if (spd < 1.0e-6f)
    {
        spd = 1.0e-6f;
    }
    vector<vec3> quads;
    for (uint8_t i = 0; i < MAX_SWEEP_STEPS; ++i)
    {
        // todo: not good like this, use cone with opening angle depending on speed, use non constant distance
        // where the sample times are defined by the cone shape so it does always overlap
        const float t = sweep_steps[i] / spd;
        if (t > cfg_sweep_time_horizon)
        {
            break;
        }
        const vec2 p = tr.pos + phy.vel * t;
        const float h = rc + cfg_v_max * t;
        quads.push_back({p.x, p.y, h});
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
                    ptrHandle, entityId, tr, bp.fatAABB, phy);
                collAvoid.nextRunFrame = ptrHandle->frameCnt + cfg_frameskip;
            }
        });
}

}  // namespace ecs
