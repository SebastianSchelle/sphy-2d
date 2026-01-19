#include <mod-manager.hpp>
#include <lua-interpreter.hpp>
#include <yaml-cpp/yaml.h>

namespace mod
{

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
                LG_D("Dependency already processed: {}", dependency);
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

    // std::this_thread::sleep_for(std::chrono::seconds(1));

    try
    {
        YAML::Node manifest = YAML::LoadFile(modInfo.manifestPath);

        if (!loadFonts(ptrHandles, modInfo))
        {
            return false;
        }
        if (!loadTextures(ptrHandles, modInfo))
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
        if (!runInitScript(ptrHandles, modInfo))
        {
            return false;
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

bool ModManager::loadShaders(PtrHandles& ptrHandles,
                             const ModInfo& modInfo,
                             YAML::Node shaders)
{
    // Defensive: shaders node must be a map
    if (!shaders.IsMap())
    {
        LG_E("Failed to load shaders: invalid node; expected a map");
        return false;
    }
    for (YAML::const_iterator it = shaders.begin(); it != shaders.end(); ++it)
    {
        if (it->first.IsScalar() && it->second.IsMap())
        {
            try
            {
                const std::string shaderName = it->first.as<std::string>();
                const std::string vsPath = modInfo.modDir + "/shader/"
                                           + it->second["vs"].as<std::string>();
                const std::string fsPath = modInfo.modDir + "/shader/"
                                           + it->second["fs"].as<std::string>();
                ptrHandles.renderEngine->loadShader(shaderName, vsPath, fsPath);
            }
            catch (const YAML::Exception& e)
            {
                LG_E("Failed to parse shader node: {}", e.what());
                return false;
            }
            catch (const std::exception& e)
            {
                LG_E("Failed to load shader '{}': {}",
                     it->first.as<std::string>(),
                     e.what());
                return false;
            }
        }
        else
        {
            LG_E(
                "Failed to load shader: invalid node; shader entry should be a "
                "map with a scalar key");
            return false;
        }
    }
    return true;
}

bool ModManager::loadFonts(PtrHandles& ptrHandles, const ModInfo& modInfo)
{
    const std::string fontsDir = modInfo.modDir + "/assets/fonts";
    if (!std::filesystem::exists(fontsDir))
    {
        LG_I("Fonts directory not found: {}", fontsDir);
        return true;
    }
    for (const auto& fileEntry : std::filesystem::directory_iterator(fontsDir))
    {
        if (fileEntry.is_regular_file()
            && fileEntry.path().extension() == ".ttf")
        {
            const std::string fontPath = fileEntry.path().string();
            if (!ptrHandles.userInterface->loadFont(fontPath))
            {
                LG_E("Failed to load font: {}", fontPath);
                return false;
            }
        }
    }
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
                    ptrHandles.renderEngine->loadTexture(
                        texName, texType, texPath);
                }
            }
        }
    }
    return true;
}


bool ModManager::loadUiDocs(PtrHandles& ptrHandles,
                            const ModInfo& modInfo,
                            YAML::Node uiDocs)
{
    // Defensive: uiDocs node must be a map
    if (!uiDocs.IsMap())
    {
        LG_E("Failed to load uiDocs: invalid node; expected a map");
        return false;
    }
    for (YAML::const_iterator it = uiDocs.begin(); it != uiDocs.end(); ++it)
    {
        if (it->first.IsScalar() && it->second.IsMap())
        {
            try
            {
                const std::string uiDocName = it->first.as<std::string>();
                const std::string uiDocPath =
                    modInfo.modDir + "/assets/ui/"
                    + it->second["path"].as<std::string>();
                ptrHandles.userInterface->loadDocument(uiDocName, uiDocPath);
            }
            catch (const YAML::Exception& e)
            {
                LG_E("Failed to parse uiDoc node: {}", e.what());
                return false;
            }
            catch (const std::exception& e)
            {
                LG_E("Failed to load uiDoc '{}': {}",
                     it->first.as<std::string>(),
                     e.what());
                return false;
            }
        }
        else
        {
            LG_E(
                "Failed to load uiDoc: invalid node; uiDoc entry should be a "
                "map with a scalar key");
            return false;
        }
    }
    return true;
}

bool ModManager::runInitScript(PtrHandles& ptrHandles, const ModInfo& modInfo)
{
    const std::string initScriptPath = modInfo.modDir + "/scripts/init.lua";
    if (!std::filesystem::exists(initScriptPath))
    {
        LG_I("Init script not found: {}", initScriptPath);
        return true;
    }
    if (!ptrHandles.luaInterpreter->runScriptFile(initScriptPath))
    {
        LG_E("Failed to run init script: {}", initScriptPath);
        return false;
    }
    return true;
}

}  // namespace mod
