#ifndef NET_SHARED_HPP
#define NET_SHARED_HPP

#include <std-inc.hpp>

namespace net
{

typedef std::function<void(const char* data, size_t length)> ReceiveCallback;

}

#endif