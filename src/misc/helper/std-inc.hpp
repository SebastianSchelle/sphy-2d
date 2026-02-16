#ifndef STD_INC_HPP
#define STD_INC_HPP

#include <array>
#include <variant>
#include <filesystem>
#include <bitsery/adapter/buffer.h>
#include <bitsery/bitsery.h>
#include <bitsery/traits/string.h>
#include <bitsery/traits/vector.h>
#include <boost/asio.hpp>
#include <boost/chrono.hpp>
#include <boost/container/vector.hpp>
#include <boost/date_time/posix_time/posix_time_config.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <glm/glm.hpp>
#include <logging.hpp>
#include <memory.h>
#include <string>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/process.hpp>
#include <fstream>

using std::string;
using std::vector;
using std::unordered_map;
namespace fs = std::filesystem;
namespace bp = boost::process;

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using udp = asio::ip::udp;


#define EXT_SER(type, block)                                                   \
    template <typename S> void serialize(S& s, type& o)                        \
    {                                                                          \
        block                                                                  \
    }

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