#ifndef CONFIG_MANAGER_HPP
#define CONFIG_MANAGER_HPP

#include "config-node.hpp"

namespace cfg {


class ConfigManager {
  public:
    ConfigManager(std::string cfgStructFile);
    ~ConfigManager();
    nodeVal_t get(std::vector<string> path) const;
    void set(std::vector<string> path, nodeVal_t value);
    void addDefs(const string& file);
  private:
    std::shared_ptr<ConfigBranch> root;
    std::vector<std::string> cfgStructFiles;
};

} // namespace cfg

#endif