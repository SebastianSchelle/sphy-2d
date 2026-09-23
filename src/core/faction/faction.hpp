#ifndef FACTION_HPP
#define FACTION_HPP

#include <std-inc.hpp>
#include <item-lib.hpp>

namespace dipl
{

class Faction
{
  public:
    struct FactionInfo
    {
        string name;
        string description;
    };

    Faction(const FactionInfo& info) : info(info) {}

  private:
    FactionInfo info;
};

using FactionHandle = typename con::ItemLib<Faction>::Handle;


}  // namespace dipl


#endif