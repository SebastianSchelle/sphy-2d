#ifndef CONFIG_MANAGER_HPP
#define CONFIG_MANAGER_HPP

#include "config-node.hpp"

#define CFG_INT(cfg_, ...)                                                     \
    static_cast<int>(std::get<float>(cfg_.get({__VA_ARGS__})));
#define CFG_UINT(cfg_, ...)                                                    \
    static_cast<uint32_t>(std::get<float>(cfg_.get({__VA_ARGS__})) >= 0.0f     \
                              ? std::get<float>(cfg_.get({__VA_ARGS__}))       \
                              : 0.0f);
#define CFG_FLOAT(cfg_, ...) std::get<float>(cfg_.get({__VA_ARGS__}));
#define CFG_STRING(cfg_, ...) std::get<string>(cfg_.get({__VA_ARGS__}));
#define CFG_BOOL(cfg_, ...) std::get<float>(cfg_.get({__VA_ARGS__}) > 0.0f);

namespace cfg
{


class ConfigManager
{
  public:
    ConfigManager(const std::string& cfgStructFile);
    ConfigManager();
    ~ConfigManager();
    void clear();
    nodeVal_t get(std::vector<string> path) const;
    void set(std::vector<string> path, nodeVal_t value);
    void addDefs(const string& file);

  private:
    void initFromFile(const string& file);

    std::shared_ptr<ConfigBranch> root;
    std::vector<std::string> cfgStructFiles;
};

}  // namespace cfg

#endif