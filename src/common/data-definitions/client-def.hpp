#ifndef CLIENT_DEF_HPP
#define CLIENT_DEF_HPP

#include <std-inc.hpp>

namespace def
{

typedef struct
{
    std::string token;
    std::string name;
    int portUdp;
    int portTcp;
    std::string address;
} ClientInfo;

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

EXT_SER(ClientInfo, s.text1b(o.token, 16); s.text1b(o.name, o.name.size());)

}  // namespace def

#endif