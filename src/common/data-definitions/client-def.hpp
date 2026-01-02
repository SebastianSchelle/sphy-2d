#ifndef CLIENT_DEF_HPP
#define CLIENT_DEF_HPP

#include <std-inc.hpp>

namespace def
{

typedef struct
{
    std::string uuid;
    std::string name;
} ClientInfo;

EXT_SER(ClientInfo, s.text1b(o.uuid, 16); s.text1b(o.name, o.name.size());)

}  // namespace def

#endif