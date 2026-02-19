#ifndef MOD_MANAGER_HPP
#define MOD_MANAGER_HPP

#ifdef CLIENT
#include <render-engine.hpp>
#include <ui/user-interface.hpp>
#endif
#include <std-inc.hpp>
#include <asset-factory.hpp>


namespace mod
{

#ifdef CLIENT
struct MenuDataMod
{
    Rml::String id;
    Rml::String name;
    Rml::String description;
    bool hasModOptions;
};
#endif

class LuaInterpreter;

struct PtrHandles
{
#ifdef CLIENT
    gfx::RenderEngine* renderEngine;
    ui::UserInterface* userInterface;
#endif
    mod::LuaInterpreter* luaInterpreter;
    ecs::AssetFactory* assetFactory;
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
    bool hasModOptions = false;
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
#ifdef CLIENT
    void populateMenuData(vector<MenuDataMod>& mods);
#endif

  private:
    bool checkDependency(const std::string& modId,
                         std::vector<std::string>& modList,
                         const std::string& modDir);
    bool checkIfDependencyProcessed(const std::string& modId);
    bool loadMod(PtrHandles& ptrHandles, const ModInfo& modInfo);
#ifdef CLIENT
    bool loadShaders(PtrHandles& ptrHandles,
                     const ModInfo& modInfo,
                     YAML::Node shaders);
    bool loadFonts(PtrHandles& ptrHandles, const ModInfo& modInfo);
    bool loadTextures(PtrHandles& ptrHandles, const ModInfo& modInfo);
    bool loadUiDocs(PtrHandles& ptrHandles,
                    const ModInfo& modInfo,
                    YAML::Node uiDocs);
    bool loadModOptions(PtrHandles& ptrHandles, ModInfo& modInfo);
#endif
    bool runInitScript(PtrHandles& ptrHandles, const ModInfo& modInfo);
    bool loadGameObjects(PtrHandles& ptrHandles, const ModInfo& modInfo);
    bool loadGameObject(PtrHandles& ptrHandles, const std::string& path);
    std::vector<ModInfo> processedDependencies;
};


}  // namespace mod

#endif