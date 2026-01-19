#ifndef MOD_MANAGER_HPP
#define MOD_MANAGER_HPP

#include <std-inc.hpp>
#include <render-engine.hpp>
#include <ui/user-interface.hpp>

namespace mod
{


class LuaInterpreter;

struct PtrHandles
{
    gfx::RenderEngine* renderEngine;
    ui::UserInterface* userInterface;
    mod::LuaInterpreter* luaInterpreter;
};

struct ModInfo
{
    std::string id;
    std::string name;
    std::string description;
    std::vector<std::string> dependencies;
    std::string manifestPath;
    std::string modDir;
    bool modFound = false;
};

class ModManager
{
  public:
    ModManager();
    ~ModManager();
    bool parseModList(const std::string& modList,
                      std::vector<std::string>& modListVec);
    bool loadMods(PtrHandles& ptrHandles);
    bool checkDependencies(std::vector<std::string>& modList,
                           const std::string& modDir);
  private:
    bool checkDependency(const std::string& modId,
                         std::vector<std::string>& modList,
                         const std::string& modDir);
    bool checkIfDependencyProcessed(const std::string& modId);
    bool loadMod(PtrHandles& ptrHandles, const ModInfo& modInfo);
    bool loadShaders(PtrHandles& ptrHandles, const ModInfo& modInfo, YAML::Node shaders);
    bool loadFonts(PtrHandles& ptrHandles, const ModInfo& modInfo);
    bool loadTextures(PtrHandles& ptrHandles, const ModInfo& modInfo);
    bool loadUiDocs(PtrHandles& ptrHandles, const ModInfo& modInfo, YAML::Node uiDocs);
    bool runInitScript(PtrHandles& ptrHandles, const ModInfo& modInfo);
    std::vector<ModInfo> processedDependencies;
};





}  // namespace mod

#endif