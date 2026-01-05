#ifndef NET_SHARED_HPP
#define NET_SHARED_HPP

#include <std-inc.hpp>
#include <item-lib.hpp>

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
    int udpPort;
    udp::endpoint udpEndpoint;
    std::shared_ptr<TcpConnection> tcpConnection;
    std::vector<uint8_t> data;
} CmdQueueData;

typedef struct
{
    std::string token;
    std::string name;
    int portUdp;
    asio::ip::address address;
    udp::endpoint udpEndpoint;
    std::shared_ptr<TcpConnection> connection;
} ClientInfo;

using ClientInfoHandle = typename con::ItemLib<ClientInfo>::Handle;

//EXT_SER(ClientInfo, s.text1b(o.token, 16); s.text1b(o.name, o.name.size());
//        s.value2b(o.portUdp);
//        s.text1b(o.address, o.address.size());)

typedef struct
{
    tim::Timepoint t0;
    tim::Timepoint t1;
    tim::Timepoint t2;
    uint8_t cnt;
    bool waiting = false;
    float latency[10];
    float offset[10];
    float serverOffset;
} TimeSync;


}  // namespace net

#endif