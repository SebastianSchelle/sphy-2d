#ifndef MOD_MANAGER_HPP
#define MOD_MANAGER_HPP

#ifdef CLIENT
#include <render-engine.hpp>
#include <ui/user-interface.hpp>
#endif
#include <asset-factory.hpp>
#include <functional>
#include <item-lib.hpp>
#include <std-inc.hpp>
#include <daScript/daScript.h>
#include <logging.hpp>

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
class Mod;

struct PtrHandles
{
#ifdef CLIENT
    gfx::RenderEngine* renderEngine;
    ui::UserInterface* userInterface;
    /// When set (e.g. mod load worker thread), RmlUi calls must run through
    /// this so they execute on the main thread. Blocks caller until the main
    /// loop runs the task.
    std::function<bool(std::function<bool()>)> runUiBool;
#endif
    mod::LuaInterpreter* luaInterpreter;
    ecs::AssetFactory* assetFactory;
    mod::Mod* currentMod;
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


typedef std::function<void(void)> ModFnLoadMod;
typedef ModFnLoadMod ModFnLoadModClient;
typedef ModFnLoadMod ModFnLoadModServer;
typedef std::function<void(void)> ModFnInitGame;
typedef ModFnInitGame ModFnInitGameClient;
typedef ModFnInitGame ModFnInitGameServer;

class ModScript
{
  public:
    ModScript(const string& path);
    ~ModScript();
    bool load();

  private:
    bool findEntryFunctions();
    string path;
    das::ProgramPtr program;
    das::ContextPtr context;
};

using ModScriptHandle = typename con::ItemLib<ModScript>::Handle;

class Mod
{
  public:
    Mod(const string& id, const string& name, const string& description);
    ~Mod();
    bool loadScript(const string& name, const string& path);
    const string& getId() const;
    const string& getName() const;
    const string& getDescription() const;

  private:
    string id;
    string name;
    string description;
    con::ItemLib<ModScript> modScripts;
};

using ModHandle = typename con::ItemLib<Mod>::Handle;

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
    bool loadScripts(PtrHandles& ptrHandles,
                     const ModInfo& modInfo,
                     YAML::Node scripts);
    std::vector<ModInfo> processedDependencies;
    con::ItemLib<Mod> modLib;
    std::vector<ModHandle> modHandles;
};

}  // namespace mod

#endif