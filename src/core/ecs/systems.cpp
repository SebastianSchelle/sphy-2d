#include <systems.hpp>

namespace ecs
{

void Systems::registerSystem(const System& system, uint16_t order)
{
    if (std::find_if(systems.begin(),
                     systems.end(),
                     [&system](const OrderedSystem& entry)
                     { return entry.system == system; })
        != systems.end())
    {
        return;
    }

    insert_sorted(
        systems,
        OrderedSystem{order, system},
        [](const OrderedSystem& a, const OrderedSystem& b)
        { return a.order < b.order; });
}

void Systems::runSystems()
{

}

}