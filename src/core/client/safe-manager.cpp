#include <exception>
#include <safe-manager.hpp>

namespace sphyc
{

void SafeManager::listSaveInfos(std::vector<SafeInfo>& safes)
{
    try
    {
        for (const auto& fileEntry :
             std::filesystem::recursive_directory_iterator(
                 options.localSaveDir))
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
                        safes.push_back(SafeInfo{
                            .name = name,
                        });
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
        LG_E("Failed to iterate over save game folder {}", options.localSaveDir);
    }
}

}  // namespace sphyc
