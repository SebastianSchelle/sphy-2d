#ifndef STD_INC_HPP
#define STD_INC_HPP

#include <array>
#include <bitsery/adapter/buffer.h>
#include <bitsery/bitsery.h>
#include <bitsery/traits/vector.h>
#include <bitsery/traits/string.h>
#include <boost/asio.hpp>
#include <boost/container/vector.hpp>
#include <filesystem>
#include <functional>
#include <iostream>
#include <logging.hpp>
#include <memory.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

using std::string;
namespace con = boost::container;

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using udp = asio::ip::udp;

enum class SendType
{
    UDP,
    TCP
};

typedef struct
{
    SendType sendType;
    char uuid[16];
    uint8_t clientId;
    std::vector<uint8_t> data;
} CmdQueueData;

#define EXT_SER(type, block)                                             \
    template <typename S> void serialize(S& s, type& o)                  \
    {                                                                          \
        block                                                                  \
    }

#endif