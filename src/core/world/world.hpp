#ifndef WORLD_HPP
#define WORLD_HPP

#include <std-inc.hpp>
#include <world-def.hpp>
#include <matrix2d.hpp>
#include <sector.hpp>
#include <config-manager.hpp>

namespace world
{

class World
{
  public:
    World();
    ~World();
    bool createFromConfig(cfg::ConfigManager &config);
    bool createFromSave(cfg::ConfigManager &config, const std::string& savedir);
    Sector* getNeighboringSector(uint32_t x, uint32_t y, def::Direction dir);
    bool saveWorld(const std::string& savedir);
  private:
    bool initWorld();
    bool initSectors(bool fromSave);
    bool loadWorldProcessData(uint32_t typeId, uint16_t version, bitsery::Deserializer<InputAdapter>& des_);

    def::WorldShape worldShape;
    con::Matrix2D<Sector> sectors;
    bool dirty;
};

}

#endif