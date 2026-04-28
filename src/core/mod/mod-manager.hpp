#ifndef MOD_MANAGER_HPP
#define MOD_MANAGER_HPP

#ifdef CLIENT
// Forward declarations only: including RmlUi here forces RTTI in
// mod-manager.cpp (RmlUi uses typeid) and breaks -fno-rtti matching with
// libDaScript.
namespace gfx
{
class RenderEngine;
}
namespace ui
{
class UserInterface;
}
#include <texture.hpp>
#endif
#include <asset-factory.hpp>
#include <functional>
#include <item-lib.hpp>
#include <std-inc.hpp>
#include <lib-hull.hpp>
#include <lib-modules.hpp>
#include <lib-textures.hpp>
#include <lib-collider.hpp>

#ifdef FMT_THROW
#pragma push_macro("FMT_THROW")
#undef FMT_THROW
#define SPHY_RESTORE_FMT_THROW 1
#endif
#include <daScript/daScript.h>
#ifdef SPHY_RESTORE_FMT_THROW
#pragma pop_macro("FMT_THROW")
#undef SPHY_RESTORE_FMT_THROW
#endif

#include <logging.hpp>

namespace mod
{

#ifdef CLIENT
struct MenuDataMod
{
    std::string id;
    std::string name;
    std::string description;
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

struct MappedTexture
{
#ifdef CLIENT
    gfx::TextureHandle texHandle;
#endif
};

using MappedTextureHandle = typename con::ItemLib<MappedTexture>::Handle;

class ResourceMap
{
  public:
    ResourceMap();
    ~ResourceMap();
    void addTexture(const string& texName, const MappedTexture& mappedTexture);
    MappedTextureHandle getTextureHandle(const string& texName) const;
    const MappedTexture*
    getMappedTexture(const MappedTextureHandle& mappedTextureHandle);

  private:
    con::ItemLib<MappedTexture> textureId;
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
    ResourceMap& getResourceMap()
    {
        return resourceMap;
    }
#ifdef CLIENT
    void populateMenuData(vector<MenuDataMod>& mods);
#endif

    con::ItemLib<gobj::Hull>& getHullLib()
    {
        return hullLib;
    }
    con::ItemLib<gobj::ModuleSlot>& getModuleSlotLib()
    {
        return moduleSlotLib;
    }
    con::ItemLib<gobj::Textures>& getTexturesLib()
    {
        return texturesLib;
    }
    con::ItemLib<gobj::MapIcon>& getMapIconLib()
    {
        return mapIconLib;
    }
    con::ItemLib<gobj::Collider>& getColliderLib()
    {
        return colliderLib;
    }
    con::ItemLib<gobj::Module>& getModuleLib()
    {
        return moduleLib;
    }

  private:
    bool checkDependency(const std::string& modId,
                         std::vector<std::string>& modList,
                         const std::string& modDir);
    bool checkIfDependencyProcessed(const std::string& modId);
    bool loadMod(PtrHandles& ptrHandles, const ModInfo& modInfo);
    bool loadTextures(PtrHandles& ptrHandles, const ModInfo& modInfo);
#ifdef CLIENT
    bool loadShaders(PtrHandles& ptrHandles,
                     const ModInfo& modInfo,
                     YAML::Node shaders);
    bool loadFonts(PtrHandles& ptrHandles, const ModInfo& modInfo);
    gfx::TextureHandle loadTextureClient(PtrHandles& ptrHandles,
                                         const string& texName,
                                         const string& texType,
                                         const string& texPath);
    bool loadUiDocs(PtrHandles& ptrHandles,
                    const ModInfo& modInfo,
                    YAML::Node uiDocs);
    bool loadModOptions(PtrHandles& ptrHandles, ModInfo& modInfo);
#endif
    bool runInitScript(PtrHandles& ptrHandles, const ModInfo& modInfo);
    bool loadGameLibs(PtrHandles& ptrHandles, const ModInfo& modInfo);
    bool loadGameLib(PtrHandles& ptrHandles, const std::string& path);
    bool loadScripts(PtrHandles& ptrHandles,
                     const ModInfo& modInfo,
                     YAML::Node scripts);
    std::vector<ModInfo> processedDependencies;
    con::ItemLib<Mod> modLib;
    std::vector<ModHandle> modHandles;
    ResourceMap resourceMap;

    con::ItemLib<gobj::Hull> hullLib;
    con::ItemLib<gobj::ModuleSlot> moduleSlotLib;
    con::ItemLib<gobj::Textures> texturesLib;
    con::ItemLib<gobj::MapIcon> mapIconLib;
    con::ItemLib<gobj::Collider> colliderLib;
    con::ItemLib<gobj::Module> moduleLib;
};

}  // namespace mod

#endif