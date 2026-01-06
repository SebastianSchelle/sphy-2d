#include "config-node.hpp"
#include "sphy-2d.hpp"

namespace cfg
{

bool yamlNodeIsString(YAML::Node& node)
{
    if (!node.IsScalar())
        return false;
    try
    {
        const string& value = node.as<string>();
        return true;
    }
    catch (std::exception& e)
    {
        return false;
    }
}

bool yamlNodeIsFloat(YAML::Node& node)
{
    if (!node.IsScalar())
        return false;
    try
    {
        const float& value = node.as<float>();
        return true;
    }
    catch (std::exception& e)
    {
        return false;
    }
}

ConfigNode::ConfigNode(const string& name) : name(name) {}

ConfigNode::~ConfigNode() {}

ConfigBranch::ConfigBranch(const string& file,
                           const string& name,
                           YAML::Node& node)
    : ConfigNode(name)
{
    if (!node.IsMap())
    {
        throw InvalidConfigItem(file, name);
    }
    LG_D("Create config branch {}", name.c_str());
    addDefs(node, file);
}

ConfigBranch::~ConfigBranch() {}

nodeVal_t ConfigBranch::get(std::vector<string>& path) const
{
    if(!path.empty())
    {
        const auto &child = children.find(path.back());
        if(child != children.end())        
        {
            path.pop_back();
            return child->second->get(path);
        }
        LG_E("Config branch {} not found", path.back())
    }
    LG_E("Config request ended in branch {}", name.c_str());
    return nodeVal_t(0.0f);
}

void ConfigBranch::set(std::vector<string>& path, nodeVal_t value)
{
    if(!path.empty())
    {
        const auto &child = children.find(path.back());
        if(child != children.end())        
        {
            path.pop_back();
            child->second->set(path, value);
            return;
        }
    }
    LG_E("Config branch {} not found", name.c_str());
}

void ConfigBranch::addDefs(YAML::Node& node, const string& file)
{
    
    for (auto it = node.begin(); it != node.end(); ++it)
    {
        const string& branchName = it->first.as<std::string>();
        if (it->second.IsMap())
        {
            children[branchName] =
                std::make_shared<ConfigBranch>(file, branchName, it->second);
        }
        else if (it->second.IsScalar())
        {
            if (yamlNodeIsString(it->second))
            {
                const string& value = it->second.as<string>();
                if (value.ends_with(".yaml"))
                {
                    std::filesystem::path currentPath(file);
                    YAML::Node cfgBranch =
                        YAML::LoadFile(currentPath.parent_path() / value);
                    children[branchName] = std::make_shared<ConfigBranch>(
                        file, branchName, cfgBranch);
                    continue;
                }
            }
            children[branchName] =
                std::make_shared<ConfigLeaf>(file, branchName, it->second);
        }
        else
        {
            throw InvalidConfigItem(file, branchName);
        }
    }
}


ConfigLeaf::ConfigLeaf(const string& file, const string& name, const YAML::Node& node)
    : ConfigNode(name)
{
    if (!node.IsScalar())
    {
        throw InvalidConfigItem(file, name);
    }
    try
    {
        value = node.as<float>();
        valueDefault = value;
        LG_D(
            "Create config leaf {} ({})", name.c_str(), std::get<float>(value));
    }
    catch (std::exception& e)
    {
        try
        {
            value = node.as<string>();
            valueDefault = value;
            LG_D("Create config leaf {} ({})",
                 name.c_str(),
                 std::get<string>(value).c_str());
        }
        catch (std::exception& e)
        {
            throw InvalidConfigItem(file, name);
        }
    }
}

ConfigLeaf::~ConfigLeaf() {}

nodeVal_t ConfigLeaf::get(std::vector<string>& path) const
{
    return value;
    LG_D("Get config leaf {} to {}", name.c_str(), std::get<float>(value));
}

void ConfigLeaf::set(std::vector<string>& path, nodeVal_t value)
{
    this->value = value;
    LG_D("Set config leaf {} to {}", name.c_str(), std::get<float>(value));
}

}  // namespace cfg
