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

using std::string;
using std::unordered_map;
using std::vector;
namespace fs = std::filesystem;
namespace bp = boost::process;

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using udp = asio::ip::udp;

using Buffer = std::vector<uint8_t>;
using OutputAdapter = bitsery::OutputBufferAdapter<Buffer>;
using InputAdapter = bitsery::InputBufferAdapter<Buffer>;

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

bool LOAD_OBJ(
    const std::string& path_,
    std::function<bool(uint32_t,
                       uint16_t,
                       bitsery::Deserializer<InputAdapter>&)> callback);

enum class ConnectionState
{
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
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

}  // namespace tim


namespace sec
{

inline std::string uuid()
{
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return std::string(uuid.begin(), uuid.end());
}

}  // namespace sec

#endif