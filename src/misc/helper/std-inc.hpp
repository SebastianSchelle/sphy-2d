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
#include <boost/process.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <logging.hpp>
#include <memory.h>
#include <string>
#include <variant>
#include <yaml-cpp/yaml.h>

using std::string;
using std::unordered_map;
using std::vector;
namespace fs = std::filesystem;
namespace bp = boost::process;

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

#define DO_PERIODIC(timekeeper_, interval_, callback_)                         \
    tim::Timepoint now = tim::getCurrentTimeU();                               \
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

#define SAPI(id) s.value2b(id)
#define S4b(value) s.value4b(value)
#define S2b(value) s.value2b(value)
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
    if (node_)                                                                 \
    {                                                                          \
        value_ = node_.as<decltype(value_)>();                                 \
    }                                                                          \
    else                                                                       \
    {                                                                          \
        value_ = default_value_;                                               \
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
    GameLoop,
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

template <> struct fmt::formatter<vector<string>>
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
};

#define SER_VEC2                                                               \
    S4b(o.x);                                                                  \
    S4b(o.y);
#define SER_VEC3                                                               \
    S4b(o.x);                                                                  \
    S4b(o.y);                                                                  \
    S4b(o.z);

EXT_SER(vec2, SER_VEC2)
EXT_DES(vec2, SER_VEC2)
EXT_SER(vec3, SER_VEC3)
EXT_DES(vec3, SER_VEC3)

EXT_FMT(vec2, "[{}, {}]", o.x, o.y);
EXT_FMT(vec3, "[{}, {}, {}]", o.x, o.y, o.z);
EXT_FMT(vec4, "[{}, {}, {}, {}]", o.x, o.y, o.z, o.w);

#endif