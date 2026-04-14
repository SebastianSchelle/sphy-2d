#ifndef WORLD_HPP
#define WORLD_HPP

#include "ecs.hpp"
#include <config-manager.hpp>
#include <matrix2d.hpp>
#include <sector.hpp>
#include <std-inc.hpp>
#include <components/comp-ident.hpp>
#include <world-def.hpp>
#ifdef CLIENT
#include <render-engine.hpp>
#endif

namespace world
{

struct SectorMoveRequest
{
  std::shared_ptr<ecs::PtrHandle> ptrHandle;
  ecs::EntityId entityId;
  uint32_t newSectorId;
};

class World
{
  public:
    World();
    ~World();
    bool createFromConfig(cfg::ConfigManager& config);
    bool createFromSave(cfg::ConfigManager& config, const std::string& savedir);
    bool createFromServer(const def::WorldShape& worldShape);
    bool getNeighboringSectorId(uint32_t sectorId, def::Direction dir, def::SectorPos& newPos);
    bool getNeighboringSectorId(uint32_t sectorX, uint32_t sectorY, def::Direction dir, def::SectorPos& newPos);
    Sector* getNeighboringSector(uint32_t x, uint32_t y, def::Direction dir);
    bool saveWorld(const std::string& savedir);
    void update(float dt, std::shared_ptr<ecs::PtrHandle> ptrHandle);
    const def::WorldShape& getWorldShape() const
    {
      return worldShape;
    }
#ifdef CLIENT
    void drawDebug(gfx::RenderEngine& renderer, float zoom);
    void drawTacticalMap(gfx::RenderEngine& renderer, const glm::vec4& viewRect, float zoom);
    void drawStrategicMap(gfx::RenderEngine& renderer, const glm::vec4& viewRect, float zoom);
    void drawThirdPerson(gfx::RenderEngine& renderer, const glm::vec4& viewRect, float zoom);
#endif
    bool moveEntityTo(std::shared_ptr<ecs::PtrHandle> ptrHandle,
                      ecs::EntityId entityId,
                      uint32_t sectorId,
                      glm::vec2 position,
                      float rotation);
    bool switchSector(std::shared_ptr<ecs::PtrHandle> ptrHandle,
                      ecs::EntityId entityId,
                      uint32_t newSectorId);
    Sector* getSector(uint32_t sectorId);
    uint32_t getSectorCount() const
    {
        return sectors.getSize();
    }
    std::pair<uint32_t, uint32_t> idToSectorCoords(uint32_t sectorId) const;
    uint32_t sectorCoordsToId(uint32_t sectorX, uint32_t sectorY) const;
    vec2 getWorldPosSectorOffset(uint32_t sectorId,
                                 int32_t sectorOffsetX,
                                 int32_t sectorOffsetY) const;
    vec2 getWorldPosSectorOffset(uint32_t sectorX,
                                 uint32_t sectorY,
                                 int32_t sectorOffsetX,
                                 int32_t sectorOffsetY) const;
    void checkSectorSwitchAfterMove(ecs::EntityId entityId,
                                    entt::entity entity,
                                    ecs::SectorId* sectorId,
                                    ecs::Transform* transform,
                                    std::shared_ptr<ecs::PtrHandle> ptrHandle);
    void addSectorMoveRequest(std::shared_ptr<ecs::PtrHandle> ptrHandle,
                              ecs::EntityId entityId,
                              uint32_t newSectorId);
    bool sectorIntersectsRect(uint32_t sectorId, const glm::vec4& rect) const;
    bool sectorIntersectsRect(
        int32_t sectorX, int32_t sectorY, const glm::vec4& rect) const;
  private:
    bool initWorld();
    bool initSectors(bool fromSave);
    bool loadWorldProcessData(uint32_t typeId,
                              uint16_t version,
                              bitsery::Deserializer<InputAdapter>& des_);
    void handleSectorMoveRequests();

    def::WorldShape worldShape;
    con::Matrix2D<Sector> sectors;
    bool dirty;
    float halfSectorSize;
    vector<SectorMoveRequest> sectorMoveRequests;
};

}  // namespace world

#endif