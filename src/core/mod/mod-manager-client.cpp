#include <mod-manager.hpp>
#include <render-engine.hpp>
#include <ui/user-interface.hpp>
#include <logging.hpp>
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <functional>
#include <string>

namespace mod
{
namespace
{

bool dispatchUiBool(PtrHandles& ptrHandles, std::function<bool()> fn)
{
    if (ptrHandles.runUiBool)
    {
        return ptrHandles.runUiBool(std::move(fn));
    }
    return fn();
}

}  // namespace

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
            if (!dispatchUiBool(
                    ptrHandles,
                    [&]()
                    { return ptrHandles.userInterface->loadFont(fontPath); }))
            {
                LG_E("Failed to load font: {}", fontPath);
                return false;
            }
        }
    }
    return true;
}

gfx::TextureHandle ModManager::loadTextureClient(PtrHandles& ptrHandles,
                                   const string& texName,
                                   const string& texType,
                                   const string& texPath)
{
    auto texHandle = ptrHandles.renderEngine->loadTexture(
        texName, texType, texPath);
    return texHandle;
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
                if (!dispatchUiBool(ptrHandles,
                                    [&]()
                                    {
                                        ptrHandles.userInterface->loadDocument(
                                            uiDocName, uiDocPath);
                                        return true;
                                    }))
                {
                    return false;
                }
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

bool ModManager::loadModOptions(PtrHandles& ptrHandles, ModInfo& modInfo)
{
    const std::string modOptionsPath =
        modInfo.modDir + "/assets/ui/mod-options.rml";
    if (std::filesystem::exists(modOptionsPath))
    {
        LG_I("Mod options found: {}", modOptionsPath);
        if (!dispatchUiBool(ptrHandles,
                            [&]()
                            {
                                ptrHandles.userInterface->loadDocument(
                                    "options-" + modInfo.id, modOptionsPath);
                                modInfo.hasModOptions = true;
                                return true;
                            }))
        {
            return false;
        }
    }
    return true;
}

void ModManager::populateMenuData(vector<MenuDataMod>& mods)
{
    mods.clear();
    for (const auto& mod : processedDependencies)
    {
        MenuDataMod modData;
        modData.id = mod.id;
        modData.name = mod.name;
        modData.description = mod.description;
        modData.hasModOptions = mod.hasModOptions;
        LG_I("Populating menu data for mod: {}", mod.id);
        mods.push_back(modData);
    }
}

}  // namespace mod
