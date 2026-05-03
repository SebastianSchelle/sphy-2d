#ifndef STD_INC_HPP
#define STD_INC_HPP

#include <array>
#include <bitsery/adapter/buffer.h>
#include <bitsery/adapter/stream.h>
#include <bitsery/bitsery.h>
#include <bitsery/traits/string.h>
#include <bitsery/traits/vector.h>
#include <boost/asio.hpp>
#include <boost/chrono.hpp>
#include <boost/container/vector.hpp>
#include <boost/date_time/posix_time/posix_time_config.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <algorithm>
#include <concurrentqueue.h>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <limits>
#include <logging.hpp>
#include <optional>
#include <memory.h>
#include <string>
#include <variant>
#include <yaml-cpp/yaml.h>

using moodycamel::ConcurrentQueue;
using std::string;
using std::unordered_map;
using std::vector;
namespace fs = std::filesystem;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using udp = asio::ip::udp;

using glm::vec2;
using glm::vec3;
using glm::vec4;
using Buffer = std::vector<uint8_t>;
using OutputAdapter = bitsery::OutputBufferAdapter<Buffer>;
using InputAdapter = bitsery::InputBufferAdapter<Buffer>;

#define DO_PERIODIC_EXTNOW(timekeeper_, interval_, now, callback_)             \
    if (tim::durationU(timekeeper_, now) > interval_)                          \
    {                                                                          \
        callback_();                                                           \
        timekeeper_ = now;                                                     \
    }
#define DO_PERIODIC_U_EXTNOW(timekeeper_, interval_, now, callback_)           \
    if ((now - timekeeper_) > interval_)                                       \
    {                                                                          \
        callback_();                                                           \
        timekeeper_ = now;                                                     \
    }

#define DO_PERIODIC(timekeeper_, interval_, callback_)                         \
    tim::Timepoint now = tim::getCurrentTimeU();                               \
    DO_PERIODIC_EXTNOW(timekeeper_, interval_, now, callback_)

#define DO_PERIODIC_U(timekeeper_, interval_, callback_)                       \
    long now = tim::nowU();                                                    \
    DO_PERIODIC_EXTNOW(timekeeper_, interval_, now, callback_)

#define TIM_1MS 1000
#define TIM_10MS 10000
#define TIM_100MS 100000
#define TIM_1S 1000000
#define TIM_10S 10000000
#define TIM_100S 100000000
#define TIM_1000S 1000000000
#define TIM_1M 60000000
#define TIM_5M 300000000
#define TIM_10M 600000000
#define TIM_1H 3600000000
#define TIM_1D 86400000000

#define TYPE_NAME_STR(T) #T
constexpr uint32_t hashConst(const char* s, size_t i = 0)
{
    return !s[i] ? 0u
                 : (hashConst(s, i + 1) * 31u + static_cast<uint32_t>(s[i]))
                       & 0xFFFFFFFFu;
}
#define TYPE_ID(T) (::hashConst(TYPE_NAME_STR(T)))
#define COMP_HASH(T) (::hashConst(T::NAME.c_str()))

#define SAPI(id) s.value2b(id)
#define S4b(value) s.value4b(value)
#define S2b(value) s.value2b(value)
#define S1b(value) s.value1b(value)
#define SOBJ(value) s.object(value)

#define EXT_SER(type, block)                                                   \
    template <typename S> void serialize(S& s, type& o)                        \
    {                                                                          \
        block                                                                  \
    }
#define EXT_DES(type, block)                                                   \
    template <typename D> void deserialize(D& s, type& o)                      \
    {                                                                          \
        block                                                                  \
    }

#define EXT_FMT(tName, strFmt, ...)                                            \
    template <> struct fmt::formatter<tName>                                   \
    {                                                                          \
        constexpr auto parse(fmt::format_parse_context& ctx)                   \
        {                                                                      \
            return ctx.begin();                                                \
        }                                                                      \
        template <typename FormatContext>                                      \
        auto format(const tName& o, FormatContext& ctx) const                  \
        {                                                                      \
            return fmt::format_to(ctx.out(), strFmt, __VA_ARGS__);             \
        }                                                                      \
    }


#define SAVE_SER_OBJECT(ser_, obj_, type_)                                     \
    ser_.value4b(TYPE_ID(type_));                                              \
    ser_.value2b(type_::VERSION);                                              \
    ser_.object(obj_);

#define SER_OBJ_PREP()                                                         \
    std::vector<uint8_t> data_;                                                \
    bitsery::Serializer<OutputAdapter> ser_(OutputAdapter{data_});
#define SER_OBJ_FIN() data_.resize(ser_.adapter().writtenBytesCount());

#define DES_OBJ_PREP(dat)                                                      \
    bitsery::Deserializer<InputAdapter> des_(                                  \
        InputAdapter{dat.begin(), dat.size()});

#define SAVE_OBJ_PREP()                                                        \
    std::vector<uint8_t> data_;                                                \
    bitsery::Serializer<OutputAdapter> ser_(OutputAdapter{data_});

#define SAVE_OBJ_FIN(path_)                                                    \
    std::ofstream file_(path_, std::ios::out | std::ios::binary);              \
    data_.resize(ser_.adapter().writtenBytesCount());                          \
    file_.write(reinterpret_cast<const char*>(data_.data()), data_.size());    \
    file_.close();

#define TRY_YAML_DICT(value_, node_, default_value_)                           \
    try                                                                        \
    {                                                                          \
        value_ = (node_).as<decltype(value_)>();                               \
    }                                                                          \
    catch (YAML::Exception e)                                                  \
    {                                                                          \
        value_ = (default_value_);                                             \
    }

bool LOAD_OBJ(
    const std::string& path_,
    std::function<bool(uint32_t,
                       uint16_t,
                       bitsery::Deserializer<InputAdapter>&)> callback);

enum class ClientGameState
{
    Init,
    LoadingMods,
    MainMenu,
    VersionCheck,
    Authenticating,
    Authenticated,
    LoadWorld,
    NotifyServerReady,
    GameLoop,
    ModdingTools,
};

namespace tim
{
const extern boost::posix_time::ptime epoch;

typedef boost::posix_time::ptime Timepoint;
typedef boost::posix_time::time_duration Duration;

inline Timepoint getCurrentTimeU()
{
    return boost::posix_time::microsec_clock::local_time();
}

inline long durationU(Timepoint t1, Timepoint t2)
{
    return (t2 - t1).total_microseconds();
}

inline long nowU()
{
    return durationU(epoch, getCurrentTimeU());
}

}  // namespace tim


namespace sec
{

inline std::string uuid()
{
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return std::string(uuid.begin(), uuid.end());
}

}  // namespace sec

namespace YAML
{
template <> struct convert<vec2>
{
    static Node encode(const vec2& v)
    {
        Node node;
        node.push_back(v.x);
        node.push_back(v.y);
        return node;
    }

    static bool decode(const Node& node, vec2& v)
    {
        if (!node.IsSequence() || node.size() != 2)
            return false;
        v.x = node[0].as<float>();
        v.y = node[1].as<float>();
        return true;
    }
};
}  // namespace YAML

/*template <> struct fmt::formatter<vector<string>>
{
    constexpr auto parse(fmt::format_parse_context& ctx)
    {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const vector<string>& strings, FormatContext& ctx) const
    {
        std::string joined;
        for (size_t i = 0; i < strings.size(); ++i)
        {
            joined += strings[i];
            if (i + 1 < strings.size())
                joined += ", ";
        }
        return fmt::format_to(ctx.out(), "[{}]", joined);
    }
};*/

// Bitsery finds serialize(S&, T&) via ADL on T. glm::vec2 lives in namespace
// glm, so these overloads must be in glm — global EXT_SER(vec2) is not visible
// to bitsery::details::HasSerializeFunction.
namespace glm
{
template <typename S> void serialize(S& s, vec2& o)
{
    s.value4b(o.x);
    s.value4b(o.y);
}
template <typename D> void deserialize(D& s, vec2& o)
{
    s.value4b(o.x);
    s.value4b(o.y);
}
template <typename S> void serialize(S& s, vec3& o)
{
    s.value4b(o.x);
    s.value4b(o.y);
    s.value4b(o.z);
}
template <typename D> void deserialize(D& s, vec3& o)
{
    s.value4b(o.x);
    s.value4b(o.y);
    s.value4b(o.z);
}
template <typename S> void serialize(S& s, vec4& o)
{
    s.value4b(o.x);
    s.value4b(o.y);
    s.value4b(o.z);
    s.value4b(o.w);
}
template <typename D> void deserialize(D& s, vec4& o)
{
    s.value4b(o.x);
    s.value4b(o.y);
    s.value4b(o.z);
    s.value4b(o.w);
}
}  // namespace glm

// Bitsery ADL note:
// std::vector lives in namespace std, so generic serialize/deserialize
// overloads for vectors must be discoverable from std during ADL.
namespace std
{
template <typename S, typename T, typename Allocator>
void serialize(S& s, vector<T, Allocator>& o)
{
    s.container(o, std::numeric_limits<std::size_t>::max());
}

template <typename D, typename T, typename Allocator>
void deserialize(D& d, vector<T, Allocator>& o)
{
    d.container(o, std::numeric_limits<std::size_t>::max());
}
}  // namespace std


namespace smath
{

inline float degToRad(float deg)
{
    return deg * M_PI / 180.0f;
}

inline float between(float d, float min, float max)
{
    return d >= min && d <= max;
}

inline float angleError(float target, float current)
{
    float error = fmodf(target - current + M_PI, 2.0f * M_PI);
    if (error < 0)
        error += 2.0f * M_PI;
    return error - M_PI;
}

inline float angleClamp(float angle)
{
    if (angle > 2 * M_PIf)
    {
        return angle - 2 * M_PIf;
    }
    else if (angle < 0.0f)
    {
        return angle + 2 * M_PIf;
    }
    return angle;
}

// Rotate a 2D vector by `radians` in the **world plane** (X right, Y down).
// Positive angle is **clockwise** (screen-space), matching `Transform::rot`
// and the SDF / tex-rect vertex shaders (same cos/sin matrix).
inline vec2 rotateVec2(const vec2& v, float s, float c)
{
    return vec2(c * v.x - s * v.y, s * v.x + c * v.y);
}

inline vec2 rotateVec2(const vec2& v, float radians)
{
    return rotateVec2(v, std::sin(radians), std::cos(radians));
}

inline vec2 perpVec2(vec2 vec)
{
    return vec2(-vec.y, vec.x);
}

inline float min(float a, float b, float c)
{
    return std::min(a, std::min(b, c));
}

typedef vec4 Rect;

inline bool intersectsRect(const Rect& rect1, const Rect& rect2)
{
    return rect1.x < rect2.z && rect1.z > rect2.x && rect1.y < rect2.w
           && rect1.w > rect2.y;
}

inline bool pointInsideRect(const vec2& point, const Rect& rect)
{
    return point.x >= rect.x && point.x <= rect.z && point.y >= rect.y
           && point.y <= rect.w;
}

}  // namespace smath

namespace ctrl
{

typedef struct
{
    float kp;
    float ki;
    float kd;
    float prev_error;
    float integral;
} PID;

void pidInit(PID* pid, float kp, float ki, float kd);
float pidCompute(PID* pid, float dt, float error);

typedef struct
{
    float kp;
    float kd;
    float prev_error;
    float setpoint;
} PD;

void pdInit(PD* pd, float kp, float kd);
float pdCompute(PD* pd, float dt, float error);
/// Same as pdCompute but does not clamp to [-1,1] (for torque limits
/// elsewhere).
float pdComputeUnclamped(PD* pd, float dt, float error);

typedef struct
{
    glm::vec2 kp;
    glm::vec2 ki;
    glm::vec2 kd;
    glm::vec2 prev_error;
    glm::vec2 integral;
} PID2D;

}  // namespace ctrl

EXT_FMT(vec2, "[{}, {}]", o.x, o.y);
EXT_FMT(vec3, "[{}, {}, {}]", o.x, o.y, o.z);
EXT_FMT(vec4, "[{}, {}, {}, {}]", o.x, o.y, o.z, o.w);

namespace fmt
{
template <typename T, typename Allocator>
struct formatter<std::vector<T, Allocator>>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const std::vector<T, Allocator>& v, FormatContext& ctx) const
    {
        auto out = ctx.out();
        *out++ = '[';
        for (std::size_t i = 0; i < v.size(); ++i)
        {
            if (i != 0)
            {
                *out++ = ',';
                *out++ = ' ';
            }
            out = fmt::format_to(out, "{}", v[i]);
        }
        *out++ = ']';
        return out;
    }
};
}  // namespace fmt


namespace sat2d
{
inline void projectOntoAxis(const std::vector<vec2>& poly,
                            const vec2& axis,
                            float& outMin,
                            float& outMax)
{
    outMin = outMax = glm::dot(poly[0], axis);
    for (size_t i = 1; i < poly.size(); ++i)
    {
        const float p = glm::dot(poly[i], axis);
        outMin = std::min(outMin, p);
        outMax = std::max(outMax, p);
    }
}

inline vec2 centroid(const std::vector<vec2>& poly)
{
    vec2 sum(0.0f);
    for (const vec2& v : poly)
    {
        sum += v;
    }
    return sum / static_cast<float>(poly.size());
}

// Convex polygon only; vertices in consistent winding (CW or CCW).
inline bool pointInConvex(const vec2& p, const std::vector<vec2>& poly)
{
    const size_t n = poly.size();
    if (n < 3)
    {
        return false;
    }
    constexpr float kEps = 1e-6f;
    std::optional<int> sign;
    for (size_t i = 0; i < n; ++i)
    {
        const vec2& a = poly[i];
        const vec2& b = poly[(i + 1) % n];
        const float cross =
            (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
        if (std::fabs(cross) < kEps)
        {
            continue;
        }
        const int s = cross > 0.0f ? 1 : -1;
        if (!sign)
        {
            sign = s;
        }
        else if (*sign != s)
        {
            return false;
        }
    }
    return true;
}

inline float convexPolygonArea(const std::vector<vec2>& poly)
{
    const size_t n = poly.size();
    if (n < 3)
    {
        return 0.0f;
    }
    float twice = 0.0f;
    for (size_t i = 0; i < n; ++i)
    {
        const vec2& v0 = poly[i];
        const vec2& v1 = poly[(i + 1) % n];
        twice += v0.x * v1.y - v0.y * v1.x;
    }
    return 0.5f * std::fabs(twice);
}

inline bool intervalsOverlapOnAxis(const std::vector<vec2>& a,
                                   const std::vector<vec2>& b,
                                   const vec2& axis,
                                   float eps)
{
    float amin;
    float amax;
    float bmin;
    float bmax;
    projectOntoAxis(a, axis, amin, amax);
    projectOntoAxis(b, axis, bmin, bmax);
    return !(amax < bmin - eps || bmax < amin - eps);
}

inline bool testAxesFromPolygon(const std::vector<vec2>& polyA,
                                const std::vector<vec2>& polyB)
{
    constexpr float kEpsLenSq = 1e-12f;
    constexpr float kEpsSep = 1e-6f;

    const size_t n = polyA.size();
    for (size_t i = 0; i < n; ++i)
    {
        const vec2& v0 = polyA[i];
        const vec2& v1 = polyA[(i + 1) % n];
        const vec2 edge = v1 - v0;
        const vec2 axis(-edge.y, edge.x);
        if (glm::dot(axis, axis) < kEpsLenSq)
        {
            continue;
        }
        if (!intervalsOverlapOnAxis(polyA, polyB, axis, kEpsSep))
        {
            return false;
        }
    }
    return true;
}

// Separating axis + minimum penetration (MTV length along chosen unit normal).
// On success, outNormal points from polygon a toward polygon b; outPenetration
// is positive overlap along that normal (world units if vertices are world).
inline bool convexConvexMTV(const std::vector<vec2>& a,
                            const std::vector<vec2>& b,
                            vec2& outNormalAToB,
                            float& outPenetration)
{
    if (a.size() < 3 || b.size() < 3)
    {
        return false;
    }

    constexpr float kEpsLenSq = 1e-12f;
    constexpr float kEpsSep = 1e-6f;

    const vec2 cA = centroid(a);
    const vec2 cB = centroid(b);

    float minOverlap = std::numeric_limits<float>::max();
    vec2 bestN(1.0f, 0.0f);

    const auto considerAxes = [&](const std::vector<vec2>& polyA,
                                  const std::vector<vec2>& polyB) -> bool
    {
        const size_t n = polyA.size();
        for (size_t i = 0; i < n; ++i)
        {
            const vec2& v0 = polyA[i];
            const vec2& v1 = polyA[(i + 1) % n];
            const vec2 edge = v1 - v0;
            const vec2 axis(-edge.y, edge.x);
            const float lenSq = glm::dot(axis, axis);
            if (lenSq < kEpsLenSq)
            {
                continue;
            }
            const vec2 unitAxis = axis * glm::inversesqrt(lenSq);

            float amin;
            float amax;
            float bmin;
            float bmax;
            projectOntoAxis(polyA, unitAxis, amin, amax);
            projectOntoAxis(polyB, unitAxis, bmin, bmax);
            const float ov = std::min(amax, bmax) - std::max(amin, bmin);
            if (ov < kEpsSep)
            {
                return false;
            }
            if (ov < minOverlap)
            {
                minOverlap = ov;
                bestN = unitAxis;
            }
        }
        return true;
    };

    if (!considerAxes(a, b) || !considerAxes(b, a))
    {
        return false;
    }

    if (minOverlap >= std::numeric_limits<float>::max() * 0.5f)
    {
        return false;
    }

    if (glm::dot(bestN, cB - cA) < 0.0f)
    {
        bestN = -bestN;
    }

    outNormalAToB = bestN;
    outPenetration = minOverlap;
    return true;
}

inline bool convexConvex(const std::vector<vec2>& a, const std::vector<vec2>& b)
{
    vec2 n;
    float pen;
    return convexConvexMTV(a, b, n, pen);
}

}  // namespace sat2d

struct GenericHandle
{
    uint16_t idx;
    uint16_t gen;
};

bool tryParseFloat(const string& text, float& outValue);
bool tryParseInt(const string& text, int& outValue);
void floatToString(float value, string& outText, int precision = 2);
void intToString(int value, string& outText);

#define SER_GENERIC_HANDLE                                                     \
    S2b(o.idx);                                                                \
    S2b(o.gen);
EXT_SER(GenericHandle, SER_GENERIC_HANDLE)
EXT_DES(GenericHandle, SER_GENERIC_HANDLE)

EXT_FMT(GenericHandle, "({}, {})", o.idx, o.gen);

using smath::Rect;

#endif