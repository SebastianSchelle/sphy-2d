#include <daScript/daScript.h>
#include <daScript/daScriptModule.h>
#include <daScript/misc/free_list.h>
#include <memory>
#include <mod-manager.hpp>
#include <sphy-bindings.hpp>
#include <yaml-cpp/yaml.h>

namespace mod
{

namespace
{

thread_local bool gDasThreadRuntimeReady = false;
std::once_flag gDasGlobalRuntimeInitOnce;

class DasLogWriter : public das::TextWriter
{
  public:
    void output() override
    {
        const uint64_t newPos = tellp();
        if (newPos == pos)
        {
            return;
        }

        std::string msg(data() + pos, size_t(newPos - pos));
        pos = newPos;

        if (msg.empty())
        {
            return;
        }

        if (msg.find("error") != std::string::npos
            || msg.find("internal error") != std::string::npos
            || msg.find("assertion failed") != std::string::npos)
        {
            LG_E("{}", msg);
        }
        else if (msg.find("warning") != std::string::npos)
        {
            LG_W("{}", msg);
        }
        else
        {
            LG_D("{}", msg);
        }

        clear();
        pos = 0;
    }

  private:
    uint64_t pos = 0;
};

class DasLoggingContext : public das::Context
{
  public:
    explicit DasLoggingContext(uint32_t stackSize) : das::Context(stackSize) {}

    void
    to_out(const das::LineInfo* at, int level, const char* message) override
    {
        (void)at;
        const std::string msg = message ? std::string(message) : std::string();
        if (msg.empty())
        {
            return;
        }

        if (level >= das::LogLevel::error)
        {
            LG_E("{}", msg);
        }
        else if (level >= das::LogLevel::warning)
        {
            LG_W("{}", msg);
        }
        else if (level >= das::LogLevel::info)
        {
            LG_I("{}", msg);
        }
        else
        {
            LG_D("{}", msg);
        }
    }
};

void ensureDasRuntimeForCurrentThread()
{
    if (gDasThreadRuntimeReady)
    {
        return;
    }

    // Thread-local daScript TLS bootstrap for the current worker thread.
    das::daScriptEnvironment::ensure();

    // Global module setup must happen exactly once process-wide.
    std::call_once(
        gDasGlobalRuntimeInitOnce,
        []()
        {
            // "daslib" = deploy/daslib; stdlib is getDasRoot()+"/daslib/"
            // (…/daslib/daslib/)
            das::setDasRoot("daslib");
            // Same set as the daslang console (incl. UriParser, JobQue, …).
            das::register_builtin_modules();
            // Register custom modules after builtins.
            mod::touchSphyBindingsModule();
            das::Module::Initialize();
            LG_D("daScript runtime initialized globally");
        });

    gDasThreadRuntimeReady = true;
}

}  // namespace

ModManager::ModManager() {}

ModManager::~ModManager() {}

bool ModManager::parseModList(const std::string& modList,
                              std::vector<std::string>& modListVec)
{
    if (!std::filesystem::exists(modList))
    {
        LG_E("Mod list file not found: {}", modList);
        return false;
    }
    LG_I("Parsing mod list from {}", modList);
    modListVec.clear();
    std::ifstream file(modList);
    std::string line;
    while (std::getline(file, line))
    {
        auto i = std::remove(line.begin(), line.end(), ' ');
        if (line.empty())
        {
            continue;
        }
        LG_I("Found mod in list: {}", line);
        modListVec.push_back(line);
    }
    file.close();
    return true;
}

bool ModManager::checkDependencies(std::vector<std::string>& modList,
                                   const std::string& modDir)
{
    processedDependencies.clear();
    std::vector<std::string> modListCpy = modList;
    while (!modListCpy.empty())
    {
        const std::string nextMod = modListCpy.front();
        if (!checkDependency(nextMod, modListCpy, modDir))
        {
            return false;
        }
    }
    return true;
}

bool ModManager::checkDependency(const std::string& modId,
                                 std::vector<std::string>& modList,
                                 const std::string& modDir)
{
    ModInfo modInfo;
    modInfo.id = modId;

    modInfo.modDir = modDir + "/" + modId;
    modInfo.manifestPath = modInfo.modDir + "/manifest.yaml";
    if (std::filesystem::exists(modInfo.manifestPath))
    {
        LG_D("Mod manifest found: {}", modInfo.manifestPath);
        modInfo.modFound = true;
        YAML::Node manifest;
        try
        {
            manifest = YAML::LoadFile(modInfo.manifestPath);
            if (manifest["deps"])
            {
                modInfo.dependencies =
                    manifest["deps"].as<std::vector<std::string>>();
            }
            if (manifest["desc"])
            {
                modInfo.description = manifest["desc"].as<std::string>();
            }
            if (manifest["name"])
            {
                modInfo.name = manifest["name"].as<std::string>();
            }
        }
        catch (const YAML::Exception& e)
        {
            LG_E("Failed to load mod manifest: {}", e.what());
            return false;
        }
        for (const auto& dependency : modInfo.dependencies)
        {
            LG_D("Found dependency: {}", dependency);
            if (checkIfDependencyProcessed(dependency))
            {
                LG_D("Dependency already processed");
            }
            else
            {
                checkDependency(dependency, modList, modDir);
            }
        }
    }
    else
    {
        LG_E("Mod manifest not found: {}", modInfo.manifestPath);
        return false;
    }
    modList.erase(std::remove(modList.begin(), modList.end(), modId),
                  modList.end());
    processedDependencies.push_back(modInfo);
    return true;
}

bool ModManager::loadMods(PtrHandles& ptrHandles)
{
    for (const auto& mod : processedDependencies)
    {
        if (!loadMod(ptrHandles, mod))
        {
            LG_E("Failed to load mod: {}", mod.id);
            return false;
        }
    }
    return true;
}

bool ModManager::checkIfDependencyProcessed(const std::string& modId)
{
    for (const auto& mod : processedDependencies)
    {
        if (mod.id == modId)
        {
            return true;
        }
    }
    return false;
}

bool ModManager::loadMod(PtrHandles& ptrHandles, const ModInfo& modInfo)
{
    LG_I("Loading mod: {}", modInfo.id);
    Mod mod(modInfo.id, modInfo.name, modInfo.description);
    ModHandle modHandle = modLib.addItem(modInfo.id, mod);
    modHandles.push_back(modHandle);
    ptrHandles.currentMod = modLib.getItem(modHandle);

    try
    {
        YAML::Node manifest = YAML::LoadFile(modInfo.manifestPath);

        if (!loadTextures(ptrHandles, modInfo))
        {
            return false;
        }
#ifdef CLIENT
        if (!loadFonts(ptrHandles, modInfo))
        {
            return false;
        }
        if (!loadModOptions(ptrHandles, const_cast<ModInfo&>(modInfo)))
        {
            return false;
        }
        // Handle shaders if present and is a map
        if (manifest["shaders"])
        {
            if (!manifest["shaders"].IsMap())
            {
                LG_E(
                    "Failed to load mod manifest: invalid node; 'shaders' "
                    "should be a map");
                return false;
            }
            if (!loadShaders(ptrHandles, modInfo, manifest["shaders"]))
            {
                return false;
            }
        }

        // Handle ui docs if present and is a map
        if (manifest["ui-docs"])
        {
            if (!manifest["ui-docs"].IsMap())
            {
                LG_E(
                    "Failed to load mod manifest: invalid node; 'ui-docs' "
                    "should be a map");
                return false;
            }
            if (!loadUiDocs(ptrHandles, modInfo, manifest["ui-docs"]))
            {
                return false;
            }
        }
#endif
        if (!loadGameLibs(ptrHandles, modInfo))
        {
            return false;
        }

        if (manifest["scripts"])
        {
            if (!manifest["scripts"].IsMap())
            {
                LG_E(
                    "Failed to load mod manifest: invalid node; 'scripts' "
                    "should be a map");
                return false;
            }
            if (!loadScripts(ptrHandles, modInfo, manifest["scripts"]))
            {
                return false;
            }
        }
    }
    catch (const YAML::Exception& e)
    {
        LG_E("Failed to load mod manifest: {}", e.what());
        return false;
    }

    LG_I("Mod loaded: {}", modInfo.id);
    return true;
}

bool ModManager::loadScripts(PtrHandles& ptrHandles,
                             const ModInfo& modInfo,
                             YAML::Node scripts)
{
    // Defensive: uiDocs node must be a map
    if (!scripts.IsMap())
    {
        LG_E("Failed to load scripts: invalid node; expected a map");
        return false;
    }
    // With DAS_FREE_LIST enabled, each worker thread that compiles scripts
    // should hold a cache guard for the duration of that compilation scope.
    das::ReuseCacheGuard reuseCacheGuard;
    ensureDasRuntimeForCurrentThread();
    for (YAML::const_iterator it = scripts.begin(); it != scripts.end(); ++it)
    {
        if (it->first.IsScalar() && it->second.IsScalar())
        {
            try
            {
                const std::string scriptName = it->first.as<std::string>();
                const std::string scriptPath =
                    modInfo.modDir + "/scripts/" + it->second.as<std::string>();
                if (!ptrHandles.currentMod->loadScript(scriptName, scriptPath))
                {
                    LG_E("Failed to load script: {}", scriptPath);
                    return false;
                }
            }
            catch (const YAML::Exception& e)
            {
                LG_E("Failed to parse script node: {}", e.what());
                return false;
            }
            catch (const std::exception& e)
            {
                LG_E("Failed to load script '{}': {}",
                     it->first.as<std::string>(),
                     e.what());
                return false;
            }
        }
        else
        {
            LG_E(
                "Failed to load script: invalid node; script entry should be a "
                "map with a scalar key");
            return false;
        }
    }
    return true;
}

bool ModManager::runInitScript(PtrHandles& ptrHandles, const ModInfo& modInfo)
{
    (void)ptrHandles;
    (void)modInfo;
    // Lua init scripts are no longer part of PtrHandles/runtime. Keep this as
    // a successful no-op for compatibility until legacy script support returns.
    return true;
}


bool ModManager::loadGameLibs(PtrHandles& ptrHandles, const ModInfo& modInfo)
{
    std::string assetsPath = modInfo.modDir + "/assets/game-objects";
    if (std::filesystem::exists(assetsPath))
    {
        for (const auto& fileEntry :
             std::filesystem::recursive_directory_iterator(assetsPath))
        {
            if (fileEntry.is_regular_file()
                && fileEntry.path().extension() == ".yaml")
            {
                const std::string gameObjectPath = fileEntry.path().string();
                if (!loadGameLib(ptrHandles, gameObjectPath))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

bool ModManager::loadGameLib(PtrHandles& ptrHandles, const std::string& path)
{
    YAML::Node libs = YAML::LoadFile(path);
    for (const auto& libEntry : libs)
    {
        bool skip = false;
        if (!libEntry.first.IsScalar() || !libEntry.second.IsMap())
        {
            LG_E("Failed to load game library: invalid node; expected a map");
            skip = true;
        }
        std::string libName = libEntry.first.as<std::string>();
        for (const auto& libEntry2 : libEntry.second)
        {
            if (libEntry2.first.IsScalar() && libEntry2.second.IsMap())
            {
                std::string objName = libEntry2.first.as<std::string>();
                if (libName == "hull")
                {
                    const gobj::Hull hull = gobj::Hull::fromYaml(
                        libEntry2.second, texturesLib, colliderLib, mapIconLib);
                    string key = hull.name != "" ? hull.name : objName;
                    hullLib.addItem(key, hull);
                    LG_I("Added hull blueprint: {}: {}", key, hull);
                }
                else if (libName == "textures")
                {
                    const gobj::Textures textures =
                        gobj::Textures::fromYaml(libEntry2.second, resourceMap);
                    texturesLib.addItem(objName, textures);
                    LG_I("Added textures: {}: {}", objName, textures);
                }
                else if (libName == "map-icon")
                {
                    const gobj::MapIcon mapIcon =
                        gobj::MapIcon::fromYaml(libEntry2.second, resourceMap);
                    mapIconLib.addItem(objName, mapIcon);
                    LG_I("Added map icon: {}: {}", objName, mapIcon);
                }
                else if (libName == "collider")
                {
                    const gobj::Collider collider =
                        gobj::Collider::fromYaml(libEntry2.second, resourceMap);
                    colliderLib.addItem(objName, collider);
                    LG_I("Added collider: {}: {}", objName, collider);
                }
                else if (libName == "module")
                {
                    const gobj::Module module =
                        gobj::Module::fromYaml(libEntry2.second, texturesLib);
                    moduleLib.addItem(objName, module);
                    LG_I("Added module: {}: {}", objName, module);
                }
                else
                {
                    LG_E("Unknown library: {}", libName);
                    skip = true;
                }
            }
        }
        if (skip)
        {
            continue;
        }
    }
    return true;
}


Mod::Mod(const std::string& id,
         const std::string& name,
         const std::string& description)
    : id(id), name(name), description(description)
{
}
Mod::~Mod() {}

bool Mod::loadScript(const string& name, const string& path)
{
    ModScript script(path);
    if (!script.load())
    {
        LG_E("Failed to load daScript file: {}", path);
        return false;
    }
    modScripts.addItem(name, script);
    return true;
}

const std::string& Mod::getId() const
{
    return id;
}

const std::string& Mod::getName() const
{
    return name;
}

const std::string& Mod::getDescription() const
{
    return description;
}

ModScript::ModScript(const string& path) : path(path) {}

ModScript::~ModScript() {}

bool ModScript::load()
{
    if (!std::filesystem::exists(path))
    {
        LG_E("Script file not found: {}", path);
        return false;
    }

    DasLogWriter tout;
    das::ModuleGroup dummyLibGroup;
    auto fAccess = das::make_smart<das::FsFileAccess>();
    program = das::compileDaScript(path, fAccess, tout, dummyLibGroup);

    if (!program)
    {
        LG_E("compileDaScript returned null program for '{}'", path);
        return false;
    }

    if (program->failed())
    {
        LG_E("daScript compilation failed: {}", path);
        for (auto& err : program->errors)
        {
            LG_E("{}",
                 reportError(err.at, err.what, err.extra, err.fixme, err.cerr));
        }
        return false;
    }

    context =
        das::ContextPtr(new DasLoggingContext(program->getContextStackSize()));
    if (!program->simulate(*context, tout))
    {
        LG_E("daScript simulation failed: {}", path);
        for (auto& err : program->errors)
        {
            LG_E("{}",
                 das::reportError(
                     err.at, err.what, err.extra, err.fixme, err.cerr));
        }
        return false;
    }

    return findEntryFunctions();
}

bool ModScript::findEntryFunctions()
{
    auto* fnAtLoad = context->findFunction("atLoad");
    if (!fnAtLoad)
    {
        LG_D("No atLoad() in '{}'", path);
        return true;
    }

    context->evalWithCatch(fnAtLoad, nullptr);
    if (auto ex = context->getException())
    {
        LG_E("Exception in atLoad() for '{}': {}", path, ex);
        return false;
    }

    LG_D("Executed atLoad() for '{}'", path);
    return true;
}

bool ModManager::loadTextures(PtrHandles& ptrHandles, const ModInfo& modInfo)
{
    const std::string texturesDir = modInfo.modDir + "/assets/textures";
    if (!std::filesystem::exists(texturesDir))
    {
        LG_I("Textures directory not found: {}", texturesDir);
        return true;
    }
    for (const auto& dirEntry :
         std::filesystem::directory_iterator(texturesDir))
    {
        if (dirEntry.is_directory())
        {
            const std::string texType = dirEntry.path().filename().string();
            const std::string path = dirEntry.path().string();
            for (const auto& fileEntry :
                 std::filesystem::directory_iterator(path))
            {
                if (fileEntry.is_regular_file()
                    && fileEntry.path().extension() == ".dds")
                {
                    const std::string texName =
                        fileEntry.path().stem().string();
                    const std::string texPath = fileEntry.path().string();
#ifdef CLIENT
                    gfx::TextureHandle texHandle = loadTextureClient(
                        ptrHandles, texName, texType, texPath);
                    resourceMap.addTexture(texName, MappedTexture(texHandle));
#endif
#ifdef SERVER
                    resourceMap.addTexture(texName, MappedTexture());
#endif
                }
            }
        }
    }
    return true;
}

ResourceMap::ResourceMap() {}
ResourceMap::~ResourceMap() {}

void ResourceMap::addTexture(const string& texName,
                             const MappedTexture& mappedTexture)
{
    MappedTextureHandle mappedTextureHandle =
        textureId.addItem(texName, mappedTexture);
    LG_I("Added mapped texture: {} with handle: {}-{}",
         texName,
         mappedTextureHandle.getIdx(),
         mappedTextureHandle.getGeneration());
}

MappedTextureHandle ResourceMap::getTextureHandle(const string& texName) const
{
    return textureId.getHandle(texName);
}

const MappedTexture*
ResourceMap::getMappedTexture(const MappedTextureHandle& mappedTextureHandle)
{
    return textureId.getItem(mappedTextureHandle);
}

}  // namespace mod

template class con::ItemLib<mod::Mod>;
