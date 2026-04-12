#ifndef NET_SHARED_HPP
#define NET_SHARED_HPP

#include <item-lib.hpp>
#include <std-inc.hpp>

namespace net
{


typedef std::function<
    void(udp::endpoint endpoint, const char* data, size_t length)>
    ReceiveCallback;
typedef std::function<void(const char* data, size_t length)> TcpReceiveCallback;

class TcpConnection;
typedef std::function<void(const char* data,
                           size_t length,
                           std::shared_ptr<TcpConnection> connection)>
    ReceiveCallbackConn;
typedef std::function<void(std::shared_ptr<TcpConnection> connection)>
    TcpDisconnectCallback;
typedef std::function<void()> ConnectionClosedCallback;

enum class SendType
{
    UDP,
    TCP
};

struct CmdQueueData
{
    SendType sendType;
    udp::endpoint udpEndpoint;
    std::shared_ptr<TcpConnection> tcpConnection;
    std::vector<uint8_t> data;
    bool tcpDisconnected = false;
};

typedef std::function<void(const net::CmdQueueData&)> TcpReceiveClb;

struct ClientFlags
{
    uint8_t enConsole : 1;
};

struct ClientInfo
{
    std::string token;
    std::string name;
    ClientFlags flags;
    int portUdp;
    asio::ip::address address;
    udp::endpoint udpEndpoint;
    std::shared_ptr<TcpConnection> connection;
    long lastSlowDump;
};

struct ModelClientInfo
{
    std::string token;
    std::string ipAddress;
    int udpPortServ;
    int tcpPortServ;
    int udpPortCli;
};

using ClientInfoHandle = typename con::ItemLib<ClientInfo>::Handle;

// EXT_SER(ClientInfo, s.text1b(o.token, 16); s.text1b(o.name, o.name.size());
//         s.value2b(o.portUdp);
//         s.text1b(o.address, o.address.size());)

struct TimeSync
{
    long t0;  // Client time at request sent
    long t1;  // Server time at request arrival
    long t2;  // Client time at response received
    uint8_t cnt = 0;
    bool waiting = false;
    long latency[10];
    long offset[10];
    float serverOffset = 0.0f;
    float serverLatency = 0.0f;
};

enum class RcvCmdState
{
  ParseCmd0,
  ParseCmd1,
  ParseLen0,
  ParseLen1,
  ParseFlags,
  ParseData,
};


}  // namespace net

#endif