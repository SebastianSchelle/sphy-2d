#include "aabb-tree.hpp"
#include "comp-collavoid.hpp"
#include "comp-phy.hpp"
#include "config-manager.hpp"
#include "glm/geometric.hpp"
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
    cfg_rb_max = CFG_FLOAT(config, 500.0f, CFG_PATH_BP, "rb_max");
    cfg_v_max = CFG_FLOAT(config, 200.0f, CFG_PATH_BP, "v_max");
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

static void collAvoidBroadphaseSweep(const Transform& tr,
                                     const con::AABB& aabb,
                                     const PhysicsBody& phy)
{
    LG_W("Broadphase sweep");
    const float ra =
        std::max(aabb.upper.x - aabb.lower.x, aabb.upper.y - aabb.lower.y);
    const float rc = ra + cfg_rb_max;
    float spd = glm::length(phy.vel);
    if(spd < 1.0e-6f)
    {
        spd = 1.0e-6f;
    }
    for (uint8_t i = 0; i < MAX_SWEEP_STEPS; ++i)
    {
        const float t = sweep_steps[i] / spd;
        if(t > cfg_sweep_time_horizon)
        {
            break;
        }
        const vec2 p = tr.pos + phy.vel * t;
        const float h = rc + cfg_rb_max * t;
        LG_D("sweep[{}]: p:{} h:{}", i, p, h);
    }
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
                collAvoidBroadphaseSweep(tr, bp.fatAABB, phy);
                collAvoid.nextRunFrame = ptrHandle->frameCnt + cfg_frameskip;
            }
        });
}

}  // namespace ecs
