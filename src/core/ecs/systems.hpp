#ifndef SYSTEMS_HPP
#define SYSTEMS_HPP

#include <std-inc.hpp>
#include <sys-defs.hpp>

namespace ecs
{

struct OrderedSystem
{
    uint16_t order;
    System system;
};

class Systems
{
  public:
    Systems() {}
    ~Systems() {}
    void registerSystem(const System& system, uint16_t order);
    void runSystems(world::Sector* sector, float dt, PtrHandle* ptrHandle);

  private:
    vector<OrderedSystem> systems;
};

};  // namespace ecs

#endif  // SYSTEMS_HPP