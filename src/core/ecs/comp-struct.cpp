#include "comp-struct.hpp"
#include "ptr-handle.hpp"
#include <lib-item.hpp>
#include <mod-manager.hpp>
#include <random>

namespace ecs
{

#ifdef SERVER

namespace
{

gobj::ItemHandle pickCompositionItem(
    const std::vector<std::pair<gobj::ItemHandle, float>>& composition)
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
    std::function<void(gobj::ItemHandle handle)> harvestCallback)
{
    hp -= damage;
    harvestProgress += damage * ptrHandle->miningRate;
    gobj::Asteroid* asteroidData =
        ptrHandle->modManager->getAsteroidLib().getItem(asteroidHandle);
    while (asteroidData && harvestProgress >= 1.0f)
    {
        const gobj::ItemHandle itemHandle =
            pickCompositionItem(asteroidData->composition);
        if (itemHandle.isValid())
        {
            harvestCallback(itemHandle);
        }
        harvestProgress -= 1.0f;
    }
}

#endif

}  // namespace ecs
