#include "config-manager/config-manager.hpp"
#include <engine.hpp>

namespace sphys
{

Engine::Engine() : configManager("defs/config-server.yaml") {}

Engine::~Engine() {}

}  // namespace sphys
