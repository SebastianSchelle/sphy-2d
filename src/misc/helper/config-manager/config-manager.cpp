#include "config-manager.hpp"

namespace cfg
{

ConfigManager::ConfigManager(const std::string& cfgStructFile)
{
    YAML::Node cfgNode = YAML::LoadFile(cfgStructFile);
    root = std::make_shared<ConfigBranch>(cfgStructFile, "root", cfgNode);
    cfgStructFiles.push_back(cfgStructFile);
}

ConfigManager::ConfigManager()
{
    root = nullptr;
    cfgStructFiles.clear();
}

ConfigManager::~ConfigManager() {}

void ConfigManager::initFromFile(const string& file)
{
    YAML::Node cfgNode = YAML::LoadFile(file);
    root = std::make_shared<ConfigBranch>(file, "root", cfgNode);
    cfgStructFiles.push_back(file);
}

void ConfigManager::addDefs(const string& file)
{
    if (!root)
    {
        initFromFile(file);
    }
    else
    {
        YAML::Node cfgNode = YAML::LoadFile(file);
        root->addDefs(cfgNode, file);
        cfgStructFiles.push_back(file);
    }
}

nodeVal_t ConfigManager::get(std::vector<string> path) const
{
    std::reverse(path.begin(), path.end());
    if (!root)
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

void ConfigManager::clear()
{
    root = nullptr;
    cfgStructFiles.clear();
}
}  // namespace cfg
