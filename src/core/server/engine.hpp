#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <sphy-2d.hpp>
#include <config-manager/config-manager.hpp>

namespace sphys {

class Engine {
  public:
    Engine();
    ~Engine();
  private:
    cfg::ConfigManager configManager;
};

} // namespace sphys

#endif