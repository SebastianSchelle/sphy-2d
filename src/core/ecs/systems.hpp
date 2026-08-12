#ifndef SYSTEMS_HPP
#define SYSTEMS_HPP

#include <ptr-handle.hpp>
#include <std-inc.hpp>

namespace ecs
{

typedef std::function<void(float, PtrHandle*)> Test;
using SystemFunction = std::variant<Test>;

enum SystemFlags
{
    ACTIVE_SECTOR = 0x0001,
    INACTIVE_SECTOR = 0x0002,
};

struct System
{
    std::string name;
    SystemFlags sysFlags;
    SystemFunction function;

    bool operator==(const System& other) const
    {
        return name == other.name;
    }
    bool operator!=(const System& other) const
    {
        return !(*this == other);
    }
};

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
    void runSystems();

  private:
    vector<OrderedSystem> systems;
};

};  // namespace ecs

#endif  // SYSTEMS_HPP