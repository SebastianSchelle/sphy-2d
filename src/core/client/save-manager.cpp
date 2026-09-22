#include <exception>
#include <save-manager.hpp>

namespace sphyc
{

void SaveManager::listSaveInfos(std::vector<SaveInfo>& saves)
{
    try
    {
        for (const auto& fileEntry :
             std::filesystem::directory_iterator(options.localSaveDir))
        {
            if (fileEntry.is_directory())
            {
                string infoPath = fileEntry.path().string() + "/info.yaml";
                try
                {
                    YAML::Node info = YAML::LoadFile(infoPath);
                    try
                    {
                        string name = info["name"].as<string>();
                        saves.push_back(
                            SaveInfo{.id = name,
                                     .name = name,
                                     .path = fileEntry.path().string()});
                    }
                    catch (YAML::Exception e)
                    {
                        LG_E("Failed to parse save info for {}", infoPath);
                    }
                }
                catch (std::exception e)
                {
                    LG_E("Failed to open save info {}", infoPath);
                }
            }
        }
    }
    catch (std::exception e)
    {
        LG_E("Failed to iterate over save game folder {}",
             options.localSaveDir);
    }
}

bool SaveManager::getLastSaved(SaveInfo& lastSave)
{
    vector<SaveInfo> saves;
    listSaveInfos(saves);
    if (!saves.size())
    {
        return false;
    }
    lastSave = saves.back();
    return true;
}

}  // namespace sphyc
