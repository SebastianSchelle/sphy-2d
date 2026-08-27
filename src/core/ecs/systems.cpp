#include <sector.hpp>
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

    insert_sorted(systems,
                  OrderedSystem{order, system},
                  [](const OrderedSystem& a, const OrderedSystem& b)
                  { return a.order < b.order; });
}

void Systems::runSystems(world::Sector* sector, float dt, PtrHandle* ptrHandle)
{
    bool sectorActive = sector->isActive();
    for (auto& system : systems)
    {
        const auto flags = system.system.sysFlags;
        if ((sectorActive && (int)(flags & SystemFlags::ActiveSector))
           || (!sectorActive && (int)(flags & SystemFlags::InactiveSector)))
        {
            system.system.function(sector, dt, ptrHandle);
        }
    }
}

}  // namespace ecs