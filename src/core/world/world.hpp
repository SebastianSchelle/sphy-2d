#ifndef WORLD_HPP
#define WORLD_HPP

#include "ecs.hpp"
#include <config-manager.hpp>
#include <matrix2d.hpp>
#include <sector.hpp>
#include <std-inc.hpp>
#include <world-def.hpp>
#ifdef CLIENT
#include <render-engine.hpp>
#endif

namespace world
{

class World
{
  public:
    World();
    ~World();
    bool createFromConfig(cfg::ConfigManager& config);
    bool createFromSave(cfg::ConfigManager& config, const std::string& savedir);
    bool createFromServer(const def::WorldShape& worldShape);
    Sector* getNeighboringSector(uint32_t x, uint32_t y, def::Direction dir);
    bool saveWorld(const std::string& savedir);
    void update(float dt, std::shared_ptr<ecs::PtrHandle> ptrHandle);
    const def::WorldShape& getWorldShape() const
    {
        return worldShape;
    }
#ifdef CLIENT
    void drawDebug(gfx::RenderEngine& renderer, float zoom);
#endif
    bool moveEntityTo(std::shared_ptr<ecs::PtrHandle> ptrHandle,
                      ecs::EntityId entityId,
                      uint32_t sectorId,
                      glm::vec2 position,
                      float rotation);

  private:
    bool initWorld();
    bool initSectors(bool fromSave);
    bool loadWorldProcessData(uint32_t typeId,
                              uint16_t version,
                              bitsery::Deserializer<InputAdapter>& des_);

    def::WorldShape worldShape;
    con::Matrix2D<Sector> sectors;
    bool dirty;
};

}  // namespace world

#endif