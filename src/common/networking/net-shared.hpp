#ifndef NET_SHARED_HPP
#define NET_SHARED_HPP

#include <std-inc.hpp>

namespace net
{


typedef std::function<void(const char* data, size_t length)> ReceiveCallback;

class TcpConnection;
typedef std::function<void(const char* data,
                           size_t length,
                           std::shared_ptr<TcpConnection> connection)>
    ReceiveCallbackConn;


enum class SendType
{
    UDP,
    TCP
};

typedef struct
{
    SendType sendType;
    uint8_t clientId;
    std::shared_ptr<TcpConnection> tcpConnection;
    std::vector<uint8_t> data;
} CmdQueueData;


}  // namespace net

#endif