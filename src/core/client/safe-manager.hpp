#ifndef SAVE_MANAGER_HPP
#define SAVE_MANAGER_HPP

#include "cmd-options.hpp"
#include "config-manager.hpp"
#include "std-inc.hpp"
namespace sphyc
{

struct SafeInfo
{
    string name;
    string path;
    tim::Timepoint safetime;
    tim::Duration playtime;
};

class SafeManager
{
  public:
    SafeManager(cfg::ConfigManager& config, sphy::CmdLinOptionsClient& options)
        : config(config), options(options)
    {
    }
    ~SafeManager() {}

    void listSaveInfos(std::vector<SafeInfo>& safes);

  private:
    sphy::CmdLinOptionsClient& options;
    cfg::ConfigManager& config;
};

}  // namespace sphyc

#endif