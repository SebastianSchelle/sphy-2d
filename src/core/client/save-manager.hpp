#ifndef SAVE_MANAGER_HPP
#define SAVE_MANAGER_HPP

#include "cmd-options.hpp"
#include "config-manager.hpp"
#include "std-inc.hpp"

namespace sphyc
{

struct SaveInfo
{
    string id;
    string name;
    string path;
    tim::Timepoint safetime;
    tim::Duration playtime;
};

class SaveManager
{
  public:
    SaveManager(cfg::ConfigManager& config, sphy::CmdLinOptionsClient& options)
        : config(config), options(options)
    {
    }
    ~SaveManager() {}

    void listSaveInfos(std::vector<SaveInfo>& safes);
    bool getLastSaved(SaveInfo& lastSave);

  private:
    sphy::CmdLinOptionsClient& options;
    cfg::ConfigManager& config;
};

}  // namespace sphyc

#endif