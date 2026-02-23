#include <ptr-handle.hpp>
#include <sector.hpp>

namespace world
{

Sector::Sector() {}

Sector::~Sector() {}

void Sector::init(int x,
                  int y,
                  float sectorSize,
                  uint32_t id,
                  Sector* neighbors[8])
{
    coordX = x;
    coordY = y;
    this->sectorSize = sectorSize;
    worldPosX = coordX * sectorSize + sectorSize / 2;
    worldPosY = coordY * sectorSize + sectorSize / 2;
    this->id = id;
    for (int i = 0; i < 8; i++)
    {
        this->neighbors[i] = neighbors[i];
    }
    dirty = true;
}

bool Sector::saveSector(const std::string& savedir)
{
    if (dirty)
    {
        std::string sectorSaveFld = savedir + "/save-data/world";
        std::string sectorFilePath = savedir + "/sector-" + std::to_string(id)
                                     + "." + GAME_NAME + ".sav";
    }
    return true;
}

void Sector::update(float dt, std::shared_ptr<ecs::PtrHandle> ptrHandle)
{
    for (auto entity : entityIds)
    {
        for (auto system : *ptrHandle->systems)
        {
            system.function(entity, dt, ptrHandle);
        }
    }
}

#ifdef CLIENT
void Sector::drawDebug(gfx::RenderEngine& renderer, float zoom)
{
    renderer.drawRectangle(
        glm::vec2(worldPosX - sectorSize / 2, worldPosY - sectorSize / 2),
        glm::vec2(sectorSize, sectorSize),
        0x10aaaa00,
        1.0f / zoom);
}
#endif

}  // namespace world