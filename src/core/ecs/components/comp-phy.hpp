#ifndef COMP_PHY_HPP
#define COMP_PHY_HPP

#include "comp-ident.hpp"
#include "ptr-handle.hpp"
#include <aabb-tree.hpp>
#include <algorithm>
#include <climits>
#include <entt/entt.hpp>
#include <lib-collider.hpp>
#include <magic_enum/magic_enum.hpp>
#include <optional>
#include <std-inc.hpp>
#include <world-def.hpp>
#include <yaml-cpp/yaml.h>

namespace mod
{
class ResourceMap;
}

namespace ecs
{
struct PtrHandle;

struct Transform
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "transform";

    vec2 pos;
    float rot;

    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node,
                         mod::ResourceMap& resourceMap)
    {
        Transform transform;
        TRY_YAML_DICT(transform.pos, node["pos"], vec2(0.0f, 0.0f));
        float rot;
        TRY_YAML_DICT(rot, node["rot"], 0.0f);
        transform.rot = smath::degToRad(rot);
        registry.emplace<Transform>(entity, transform);
    }
};

#define SER_TRANSFORM                                                          \
    SOBJ(o.pos);                                                               \
    S4b(o.rot);

EXT_SER(Transform, SER_TRANSFORM)
EXT_DES(Transform, SER_TRANSFORM)

struct TransformCache
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "transform-cache";

    float c;
    float s;

    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node,
                         mod::ResourceMap& resourceMap)
    {
        TransformCache transformCache;
        TRY_YAML_DICT(transformCache.c, node["c"], 0.0f);
        TRY_YAML_DICT(transformCache.s, node["s"], 0.0f);
        registry.emplace<TransformCache>(entity, transformCache);
    }
};

#define SER_TRANSFORM_CACHE                                                    \
    S4b(o.c);                                                                  \
    S4b(o.s);
EXT_SER(TransformCache, SER_TRANSFORM_CACHE)
EXT_DES(TransformCache, SER_TRANSFORM_CACHE)

// Uses vec2 (glm) for components; same role as a hypothetical Vec2 type.
struct Contact
{
    vec2 normal{};        // unit, direction from collider A → collider B
    float penetration{};  // positive overlap along normal (same units as verts)
    vec2 point{};  // approximate contact (centroid midpoint); refine later
};

struct ContactInfo
{
    Contact contact;
    entt::entity ent1;
    entt::entity ent2;
    float restitution;
};

struct Collider
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "collider";

    GenericHandle colliderHandle;

    const gobj::Collider*
    getColliderDef(con::ItemLib<gobj::Collider>* colliderLib) const
    {
        if (!colliderLib)
        {
            return nullptr;
        }
        return colliderLib->getItem(gobj::ColliderHandle(colliderHandle));
    }

    const std::vector<vec2>*
    getVertices(con::ItemLib<gobj::Collider>* colliderLib) const
    {
        return getVertices(getColliderDef(colliderLib));
    }

    float getRestitution(con::ItemLib<gobj::Collider>* colliderLib) const
    {
        return getRestitution(getColliderDef(colliderLib));
    }

    const std::vector<vec2>*
    getVertices(const gobj::Collider* colliderDef) const
    {
        if (colliderDef)
        {
            return &colliderDef->vertices;
        }
        return nullptr;
    }

    float getRestitution(const gobj::Collider* colliderDef) const
    {
        if (colliderDef)
        {
            return colliderDef->restitution;
        }
        return 0.5f;
    }

    // worldPoint and tr.pos are in the same space (e.g. sector-local when
    // sector matches). c,s must match TransformCache / cos(rot),sin(rot) used
    // by collideCollidersWorld.
    bool isPointInsideWorld(
        const vec2& worldPoint,
        const Transform& tr,
        float c,
        float s,
        con::ItemLib<gobj::Collider>* colliderLib = nullptr) const
    {
        const auto* verts = getVertices(colliderLib);
        if (!verts || verts->size() < 3)
        {
            return false;
        }
        const float dx = worldPoint.x - tr.pos.x;
        const float dy = worldPoint.y - tr.pos.y;
        const vec2 local(c * dx + s * dy, -s * dx + c * dy);
        return sat2d::pointInConvex(local, *verts);
    }

    bool isPointInsideWorld(
        const vec2& worldPoint,
        const Transform& tr,
        const TransformCache& tc,
        con::ItemLib<gobj::Collider>* colliderLib = nullptr) const
    {
        return isPointInsideWorld(worldPoint, tr, tc.c, tc.s, colliderLib);
    }
};

// World-space SAT + contact: same rotation/translation as calculateAABB.
inline std::optional<Contact> collideCollidersWorld(const Collider& c1,
                                                    const gobj::Collider* c1Def,
                                                    const Transform& t1,
                                                    const TransformCache& tc1,
                                                    const Collider& c2,
                                                    const gobj::Collider* c2Def,
                                                    const Transform& t2,
                                                    const TransformCache& tc2);
inline std::optional<Contact>
collideCollidersWorld(const Collider& c1,
                      const Transform& t1,
                      const TransformCache& tc1,
                      const Collider& c2,
                      const Transform& t2,
                      const TransformCache& tc2,
                      con::ItemLib<gobj::Collider>* colliderLib = nullptr)
{
    return collideCollidersWorld(c1,
                                 c1.getColliderDef(colliderLib),
                                 t1,
                                 tc1,
                                 c2,
                                 c2.getColliderDef(colliderLib),
                                 t2,
                                 tc2);
}

inline std::optional<Contact> collideCollidersWorld(const Collider& c1,
                                                    const gobj::Collider* c1Def,
                                                    const Transform& t1,
                                                    const TransformCache& tc1,
                                                    const Collider& c2,
                                                    const gobj::Collider* c2Def,
                                                    const Transform& t2,
                                                    const TransformCache& tc2)
{
    const auto* v1 = c1.getVertices(c1Def);
    const auto* v2 = c2.getVertices(c2Def);
    if (!v1 || !v2)
    {
        return std::nullopt;
    }
    const size_t n1 = v1->size();
    const size_t n2 = v2->size();
    if (n1 < 3 || n2 < 3)
    {
        return std::nullopt;
    }

    thread_local std::vector<vec2> w1;
    thread_local std::vector<vec2> w2;
    w1.resize(n1);
    w2.resize(n2);

    for (size_t i = 0; i < n1; ++i)
    {
        const vec2& v = (*v1)[i];
        w1[i].x = tc1.c * v.x - tc1.s * v.y + t1.pos.x;
        w1[i].y = tc1.s * v.x + tc1.c * v.y + t1.pos.y;
    }
    for (size_t i = 0; i < n2; ++i)
    {
        const vec2& v = (*v2)[i];
        w2[i].x = tc2.c * v.x - tc2.s * v.y + t2.pos.x;
        w2[i].y = tc2.s * v.x + tc2.c * v.y + t2.pos.y;
    }

    vec2 n;
    float pen;
    if (!sat2d::convexConvexMTV(w1, w2, n, pen))
    {
        return std::nullopt;
    }
    Contact c;
    c.normal = n;
    c.penetration = pen;
    c.point = 0.5f * (sat2d::centroid(w1) + sat2d::centroid(w2));
    return c;
}

#define SER_COLLIDER SOBJ(o.colliderHandle);
EXT_SER(Collider, SER_COLLIDER)
EXT_DES(Collider, SER_COLLIDER)

struct Broadphase
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "broadphase";

    int32_t proxyId;
    con::AABB fatAABB;

    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node,
                         mod::ResourceMap& resourceMap)
    {
        Broadphase broadphase;
        registry.emplace<Broadphase>(entity, broadphase);
    }
};

#define SER_BROADPHASE                                                         \
    S4b(o.proxyId);                                                            \
    SOBJ(o.fatAABB);
EXT_SER(Broadphase, SER_BROADPHASE)
EXT_DES(Broadphase, SER_BROADPHASE)

inline con::AABB calculateAABB(const Transform& transform,
                               const TransformCache& transformCache,
                               const Collider& collider,
                               const gobj::Collider* colliderDef);
inline con::AABB
calculateAABB(const Transform& transform,
              const TransformCache& transformCache,
              const Collider& collider,
              con::ItemLib<gobj::Collider>* colliderLib = nullptr)
{
    return calculateAABB(transform,
                         transformCache,
                         collider,
                         collider.getColliderDef(colliderLib));
}

inline con::AABB calculateAABB(const Transform& transform,
                               const TransformCache& transformCache,
                               const Collider& collider,
                               const gobj::Collider* colliderDef)
{
    con::AABB aabb =
        con::AABB{vec2{1.0e10f, 1.0e10f}, vec2{-1.0e10f, -1.0e10f}};
    const auto* verts = collider.getVertices(colliderDef);
    if (!verts)
    {
        return aabb;
    }
    for (const auto& vert : *verts)
    {
        float x = transformCache.c * vert.x - transformCache.s * vert.y
                  + transform.pos.x;
        float y = transformCache.s * vert.x + transformCache.c * vert.y
                  + transform.pos.y;
        aabb.lower = con::minVec(aabb.lower, vec2{x, y});
        aabb.upper = con::maxVec(aabb.upper, vec2{x, y});
    }
    return aabb;
}

struct TransformNet
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "trans-net";

    TransformNet(Transform transform)
    {
        this->transform = transform;
    }
    Transform transform;
    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node,
                         mod::ResourceMap& resourceMap)
    {
        Transform transform;
        transform.pos = node["pos"].as<vec2>();
        transform.rot = node["rot"].as<float>();
        TransformNet transformNet(transform);
        registry.emplace<TransformNet>(entity, transformNet);
    }
};

#define SER_TRANSFORM_NET SOBJ(o.transform);
EXT_SER(TransformNet, SER_TRANSFORM_NET)
EXT_DES(TransformNet, SER_TRANSFORM_NET)

struct PhysicsBody
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "physics-body";

    float mass = 1.0f;        // kg
    vec2 vel = {0.0f, 0.0f};  // m/s
    vec2 acc = {0.0f, 0.0f};  // m/s^2
    float inertia = 1.0f;     // kg*m^2
    float rotVel = 0.0f;      // rad/s
    float rotAcc = 0.0f;      // rad/s^2

    void applyForce(vec2 force)
    {
        acc += force / mass;
    }
    void applyTorque(float torque)
    {
        rotAcc += torque / inertia;
    }
    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node,
                         mod::ResourceMap& resourceMap)
    {
        PhysicsBody physicsBody;
        TRY_YAML_DICT(physicsBody.mass, node["mass"], 1.0f);
        TRY_YAML_DICT(physicsBody.inertia, node["inertia"], 1.0f);
        TRY_YAML_DICT(physicsBody.vel, node["vel"], vec2(0.0f, 0.0f));
        TRY_YAML_DICT(physicsBody.rotVel, node["rotVel"], 0.0f);
        TRY_YAML_DICT(physicsBody.rotAcc, node["rotAcc"], 0.0f);
        TRY_YAML_DICT(physicsBody.acc, node["acc"], vec2(0.0f, 0.0f));
        if (physicsBody.inertia <= 1e-8f)
        {
            physicsBody.inertia = 1.0f;
        }
        if (physicsBody.mass <= 1e-8f)
        {
            physicsBody.mass = 1.0f;
        }
        registry.emplace<PhysicsBody>(entity, physicsBody);
    }
};

#define SER_PHYSICS_BODY                                                       \
    S4b(o.mass);                                                               \
    S4b(o.inertia);                                                            \
    SOBJ(o.vel);                                                               \
    S4b(o.rotVel);                                                             \
    S4b(o.rotAcc);                                                             \
    SOBJ(o.acc);
EXT_SER(PhysicsBody, SER_PHYSICS_BODY)
EXT_DES(PhysicsBody, SER_PHYSICS_BODY)

/// Scale local thrust uniformly so |x|<=maneuverMax, |y|<=mainMax; preserves
/// direction (per-axis clamp does not). Hot path: already inside box (no div).
inline void
clampThrustLocalToActuatorBox(vec2& local, float maneuverMax, float mainMax)
{
    constexpr float eps = 1e-12f;
    const float tx = local.x;
    const float ty = local.y;
    const float ax = std::fabsf(tx);
    const float ay = std::fabsf(ty);
    if (ax < eps && ay < eps)
    {
        local = vec2(0.0f);
        return;
    }
    if (ax <= maneuverMax && ay <= mainMax)
    {
        return;
    }
    float s = 1.0f;
    if (ax > eps)
    {
        s = std::fminf(s, maneuverMax / ax);
    }
    if (ay > eps)
    {
        s = std::fminf(s, mainMax / ay);
    }
    local.x = tx * s;
    local.y = ty * s;
}

struct PhyThrust
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "phy-thrust";

    glm::vec2 thrustGlobal;
    glm::vec2 thrustLocal;
    float torque;
    float maxTorque;
    float maxRotVel;
    float thrustMainMax;
    float thrustManeuverMax;
    float maxSpd;

    void setThrustGlobal(vec2 th, Transform& tr)
    {
        // World -> body: inverse of CW rotation by tr.rot
        const float s = std::sin(tr.rot);
        const float c = std::cos(tr.rot);
        setThrustGlobal(th, s, c);
    }

    void setThrustGlobal(vec2 th, float s, float c)
    {
        setThrustLocal(smath::rotateVec2(th, -s, c), s, c);
    }

    void setThrustLocal(vec2 th, Transform& tr)
    {
        const float s = std::sin(tr.rot);
        const float c = std::cos(tr.rot);
        setThrustLocal(th, s, c);
    }

    void setThrustLocal(vec2 th, float s, float c)
    {
        thrustLocal = th;
        clampThrustLocalToActuatorBox(
            thrustLocal, thrustManeuverMax, thrustMainMax);
        thrustGlobal = smath::rotateVec2(thrustLocal, s, c);
    }

    void setThrustNone()
    {
        thrustGlobal = vec2(0.0f);
        thrustLocal = vec2(0.0f);
        torque = 0.0f;
    }

    void setTorque(float trq)
    {
        trq = std::clamp(trq, -maxTorque, maxTorque);
        torque = trq;
    }
    void setTorqueNorm(float trq)
    {
        trq = std::clamp(trq, -1.0f, 1.0f);
        torque = trq * maxTorque;
    }

    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node,
                         mod::ResourceMap& resourceMap)
    {
        PhyThrust phyThrust;
        TRY_YAML_DICT(phyThrust.thrustGlobal,
                      node["thrustGlobal"],
                      glm::vec2(0.0f, 0.0f));
        TRY_YAML_DICT(
            phyThrust.thrustLocal, node["thrustLocal"], glm::vec2(0.0f, 0.0f));
        TRY_YAML_DICT(phyThrust.torque, node["torque"], 0.0f);
        TRY_YAML_DICT(phyThrust.maxTorque, node["maxTorque"], 0.0f);
        TRY_YAML_DICT(phyThrust.maxRotVel, node["maxRotVel"], 0.0f);
        TRY_YAML_DICT(phyThrust.thrustMainMax, node["thrustMainMax"], 0.0f);
        TRY_YAML_DICT(
            phyThrust.thrustManeuverMax, node["thrustManeuverMax"], 0.0f);
        TRY_YAML_DICT(phyThrust.maxSpd, node["maxSpd"], 0.0f);
        registry.emplace<PhyThrust>(entity, phyThrust);
    }

    void updateStatsFromEntity(entt::entity entity, ecs::PtrHandle* ptrHandle);
};

#define SER_PHY_THRUST                                                         \
    SOBJ(o.thrustGlobal);                                                      \
    SOBJ(o.thrustLocal);                                                       \
    S4b(o.torque);                                                             \
    S4b(o.maxTorque);                                                          \
    S4b(o.maxRotVel);                                                          \
    S4b(o.thrustMainMax);                                                      \
    S4b(o.thrustManeuverMax);                                                  \
    S4b(o.maxSpd);
EXT_SER(PhyThrust, SER_PHY_THRUST)
EXT_DES(PhyThrust, SER_PHY_THRUST)

struct MoveCtrl
{
    struct MCForwardData
    {
        float minFaceForwardDist;
    };

    struct MCTargetPointData
    {
        vec2 lookAt;
    };
    enum class FaceDirMode : uint8_t
    {
        None,
        Forward,
        TargetPoint,
    };

    static const uint16_t VERSION = 1;
    static constexpr string NAME = "move-ctrl";

    bool active = false;
    bool posReached = false;
    bool rotReached = false;
    FaceDirMode faceDirMode;
    def::SectorCoords spPos;
    // lookAt only works in sector
    vec2 lookAt;
    float spRot;
    float allowedPosError;
    float allowedRotError;

    std::variant<MCForwardData, MCTargetPointData> faceDirData =
        MCForwardData{100.0f};

    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node,
                         mod::ResourceMap& resourceMap)
    {
        MoveCtrl c;
        string dirMode;
        TRY_YAML_DICT(c.active, node["active"], false);
        TRY_YAML_DICT(c.posReached, node["targetReached"], false);
        TRY_YAML_DICT(c.spPos.sectorPos.x, node["spPos"][0], 0.0f);
        TRY_YAML_DICT(c.spPos.sectorPos.y, node["spPos"][1], 0.0f);
        TRY_YAML_DICT(c.spPos.pos.x, node["spPosSec"][0], 0u);
        TRY_YAML_DICT(c.spPos.pos.y, node["spPosSec"][1], 0u);
        float rot;
        TRY_YAML_DICT(rot, node["spRot"], 0.0f);
        c.spRot = smath::degToRad(rot);
        TRY_YAML_DICT(c.allowedPosError, node["allowedPosError"], 100.0f);
        float allRotErr;
        TRY_YAML_DICT(allRotErr, node["allowedRotError"], 5.0f);
        c.allowedRotError = smath::degToRad(allRotErr);
        TRY_YAML_DICT(dirMode, node["faceDirMode"], "None");
        auto faceDirMode = magic_enum::enum_cast<FaceDirMode>(dirMode);
        if (faceDirMode.has_value())
        {
            c.faceDirMode = faceDirMode.value();
        }
        else
        {
            c.faceDirMode = FaceDirMode::None;
        }
        switch (c.faceDirMode)
        {
            case FaceDirMode::Forward:
                float minFFDist;
                TRY_YAML_DICT(minFFDist, node["minFFDist"], 100.0f);
                c.faceDirData = MCForwardData{minFFDist};
                break;
            case FaceDirMode::TargetPoint:
                vec2 lookAt;
                TRY_YAML_DICT(lookAt, node["lookAt"], vec2(0.0f, 0.0f));
                c.faceDirData = MCTargetPointData{vec2(0.0f, 0.0f)};
                break;
            default:
                break;
        }
        TRY_YAML_DICT(c.lookAt.x, node["lookAt"][0], 0.0f);
        TRY_YAML_DICT(c.lookAt.y, node["lookAt"][1], 0.0f);
        registry.emplace<MoveCtrl>(entity, c);
    }
};

#define SER_MOVE_CTRL_HOLD                                                     \
    S1b(o.active);                                                             \
    S1b(o.posReached);                                                         \
    S1b(o.rotReached);                                                         \
    SOBJ(o.spPos);                                                             \
    S4b(o.spRot);                                                              \
    S1b(o.faceDirMode);                                                        \
    SOBJ(o.lookAt);                                                            \
    S4b(o.allowedPosError);                                                    \
    S4b(o.allowedRotError);
EXT_SER(MoveCtrl, SER_MOVE_CTRL_HOLD)
EXT_DES(MoveCtrl, SER_MOVE_CTRL_HOLD)

struct AnchorFixed
{
    static const uint16_t VERSION = 1;
    static constexpr string NAME = "anchor-fixed";

    vec2 pos;
    float rot;
    EntityId ref;

    static void fromYaml(entt::registry& registry,
                         entt::entity entity,
                         const YAML::Node& node,
                         mod::ResourceMap& resourceMap)
    {
        AnchorFixed anchorFixed;
        TRY_YAML_DICT(anchorFixed.pos, node["pos"], vec2(0.0f, 0.0f));
        float rot;
        TRY_YAML_DICT(rot, node["rot"], 0.0f);
        anchorFixed.rot = smath::degToRad(rot);
        uint16_t refIdx;
        uint16_t refGen;
        TRY_YAML_DICT(refIdx, node["ref"][0], 0);
        TRY_YAML_DICT(refGen, node["ref"][1], 0);
        anchorFixed.ref = {refIdx, refGen};
        registry.emplace<AnchorFixed>(entity, anchorFixed);
    }
};

#define SER_ANCHOR_FIXED                                                       \
    SOBJ(o.pos);                                                               \
    S4b(o.rot);                                                                \
    SOBJ(o.ref);
EXT_SER(AnchorFixed, SER_ANCHOR_FIXED)
EXT_DES(AnchorFixed, SER_ANCHOR_FIXED)

}  // namespace ecs

EXT_FMT(ecs::Transform, "(pos: {}, rot: {})", o.pos, o.rot);

EXT_FMT(ecs::TransformNet, "{}", o.transform);

EXT_FMT(ecs::PhysicsBody,
        "(mass: {}, inertia: {}, vel: {}, acc: {}, rotVel: {}, rotAcc: {})",
        o.mass,
        o.inertia,
        o.vel,
        o.acc,
        o.rotVel,
        o.rotAcc);
EXT_FMT(ecs::PhyThrust,
        "(thrustGlobal: {}, thrustLocal: {}, torque: {}, maxTorque: {}, "
        "maxRotVel: {}, thrustMainMax: {}, thrustManeuverMax: {}, maxSpd: {})",
        o.thrustGlobal,
        o.thrustLocal,
        o.torque,
        o.maxTorque,
        o.maxRotVel,
        o.thrustMainMax,
        o.thrustManeuverMax,
        o.maxSpd);
EXT_FMT(ecs::MoveCtrl,
        "(active: {}, targetReached: {}, spPos: {}, spRot: {}, faceDirMode: "
        "{}, lookAt: {})",
        o.active,
        o.posReached,
        o.spPos,
        o.spRot,
        magic_enum::enum_name(o.faceDirMode),
        o.lookAt);

EXT_FMT(ecs::Collider, "(colliderHandle: {})", o.colliderHandle);
EXT_FMT(ecs::Broadphase, "(proxyId: {}, fatAABB: {})", o.proxyId, o.fatAABB);
EXT_FMT(ecs::TransformCache, "(c: {}, s: {})", o.c, o.s);
EXT_FMT(con::AABB, "(lower: {}, upper: {})", o.lower, o.upper);
EXT_FMT(ecs::AnchorFixed, "(pos: {}, rot: {}, ref: {})", o.pos, o.rot, o.ref);

#endif