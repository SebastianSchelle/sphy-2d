#ifndef CONFIG_NODE_HPP
#define CONFIG_NODE_HPP

#include <std-inc.hpp>
#include <yaml-cpp/yaml.h>

namespace cfg
{

typedef std::variant<float, string> nodeVal_t;

bool yamlNodeIsString(YAML::Node& node);
bool yamlNodeIsFloat(YAML::Node& node);

class InvalidConfigItem : public std::exception
{
  public:
    InvalidConfigItem(const string& file, const string& nodeName) throw()
        : nodeName(nodeName),
          message("Invalid config item " + nodeName + " in file " + file)
    {
    }
    virtual ~InvalidConfigItem() throw() {}
    const char* what() const throw()
    {
        return message.c_str();
    }

  private:
    const std::string& nodeName;
    std::string message;
};

class ConfigNode
{
  public:
    ConfigNode(const string& name);
    virtual ~ConfigNode();
    virtual nodeVal_t get(std::vector<string>& path) const = 0;
    virtual void set(std::vector<string>& path, nodeVal_t value) = 0;
  protected:
    const string name;
};

class ConfigBranch : public ConfigNode
{
  public:
    ConfigBranch(const string& file, const string& name, YAML::Node& node);
    ~ConfigBranch();
    nodeVal_t get(std::vector<string>& path) const override;
    void set(std::vector<string>& path, nodeVal_t value) override;
    void addDefs(YAML::Node& node, const string& file);
  private:
    std::unordered_map<string, std::shared_ptr<ConfigNode>> children;
};

class ConfigLeaf : public ConfigNode
{
  public:
    ConfigLeaf(const string& file, const string& name, const YAML::Node& node);
    ~ConfigLeaf();
    nodeVal_t get(std::vector<string>& path) const override;
    void set(std::vector<string>& path, nodeVal_t value) override;
  private:
    nodeVal_t valueDefault;
    nodeVal_t value;
};

}  // namespace cfg

#endif