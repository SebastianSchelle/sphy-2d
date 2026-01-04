#include "config-manager.hpp"

namespace cfg
{

ConfigManager::ConfigManager(std::string cfgStructFile)
{
    YAML::Node cfgNode = YAML::LoadFile(cfgStructFile);
    root = std::make_shared<ConfigBranch>(cfgStructFile, "root", cfgNode);
    cfgStructFiles.push_back(cfgStructFile);
}

ConfigManager::~ConfigManager() {}

void ConfigManager::addDefs(const string& file)
{
    YAML::Node cfgNode = YAML::LoadFile(file);
    root->addDefs(cfgNode, file);
    cfgStructFiles.push_back(file);
}

nodeVal_t ConfigManager::get(std::vector<string> path) const
{
    std::reverse(path.begin(), path.end());
    if(!root)
    {
        LG_E("Config manager root is not initialized");
        return nodeVal_t(0.0f);
    }
    return root->get(path);
}

void ConfigManager::set(std::vector<string> path, nodeVal_t value)
{
    std::reverse(path.begin(), path.end());
    root->set(path, value);
}
}  // namespace cfg
