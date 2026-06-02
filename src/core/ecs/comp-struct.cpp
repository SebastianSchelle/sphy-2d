#include "comp-struct.hpp"
#include "ptr-handle.hpp"
#include <lib-asteroid.hpp>
#include <lib-item.hpp>
#include <mod-manager.hpp>
#include <random>

namespace ecs
{

#ifdef SERVER

namespace
{

gobj::ItemHandle
pickCompositionItem(const gobj::AsteroidComposition& composition)
{
    float totalWeight = 0.0f;
    for (const auto& [handle, fraction] : composition)
    {
        if (handle.isValid() && fraction > 0.0f)
        {
            totalWeight += fraction;
        }
    }
    if (totalWeight <= 0.0f)
    {
        return gobj::ItemHandle::Invalid();
    }

    thread_local std::mt19937 gen{std::random_device{}()};
    std::uniform_real_distribution<float> dist(0.0f, totalWeight);
    const float pick = dist(gen);

    float cumulative = 0.0f;
    for (const auto& [handle, fraction] : composition)
    {
        if (!handle.isValid() || fraction <= 0.0f)
        {
            continue;
        }
        cumulative += fraction;
        if (pick < cumulative)
        {
            return handle;
        }
    }
    return gobj::ItemHandle::Invalid();
}

}  // namespace

void Asteroid::damage(
    PtrHandle* ptrHandle,
    float damage,
    std::function<void(gobj::ItemHandle handle, uint32_t quantity)> harvestCallback)
{
    volume -= damage;
    gobj::Asteroid* asteroidData =
        ptrHandle->modManager->getAsteroidLib().getItem(
            gobj::AsteroidHandle(asteroidHandle));
    if (!asteroidData)
    {
        return;
    }
    switch (asteroidData->type)
    {
        case gobj::AsteroidType::Fragment:
        {
            auto& fragment =
                std::get<gobj::AsteroidFragmentdata>(asteroidData->content);
            harvestProgress += damage;
            while (harvestProgress >= 10.0f)
            {
                const gobj::ItemHandle itemHandle =
                    pickCompositionItem(fragment.composition);
                if (itemHandle.isValid())
                {
                    harvestCallback(itemHandle, 10);
                }
                harvestProgress -= 10.0f;
            }
            break;
        }
        case gobj::AsteroidType::Parent:
        {
            break;
        }
        default:
            break;
    }
}

#endif

}  // namespace ecs
