#ifndef CMD_OPTIONS_HPP
#define CMD_OPTIONS_HPP

#include "os-helper.hpp"
#include <boost/program_options.hpp>
#include <functional>
#include <iostream>

namespace po = boost::program_options;

namespace sphy
{
class CmdLineOptions
{
  public:
    CmdLineOptions() {}
    bool parse(po::variables_map& vm)
    {
        if (vm.count("moddir"))
        {
            modDirSet = true;
            moddir = vm["moddir"].as<std::string>();
        }
        else
        {
            moddir = ".";
        }
        if (vm.count("configdir"))
        {
            configdir = vm["configdir"].as<std::string>();
            configDirSet = true;
        }
        else
        {
            configdir = ".";
        }
        if (vm.count("workingdir"))
        {
            workingdir = vm["workingdir"].as<std::string>();
            workingDirSet = true;
        }
        else
        {
            workingdir = ".";
        }
        if (vm.count("bindir"))
        {
            bindir = vm["bindir"].as<std::string>();
        }
        else
        {
            bindir = osh::executablePath().parent_path();
        }


        if (!configDirSet)
        {
            configdir = bindir;
        }
        if (!workingDirSet)
        {
            workingdir = bindir;
        }
        if (!modDirSet)
        {
            moddir = bindir + "/modules";
        }
        return true;
    }

    static void createCmdLineOptions(po::options_description& desc)
    {
        desc.add_options()("help,h", "Print help message")(
            "moddir,m", po::value<std::string>(), "Mod directory")(
            "configdir,c", po::value<std::string>(), "Config directory")(
            "bindir,b", po::value<std::string>(), "Binary directory")(
            "workingdir,w", po::value<std::string>(), "Working directory");
    }


    static bool handleDefaultCmdLineOptions(int argc,
                                            char* argv[],
                                            po::options_description& desc,
                                            po::variables_map& vm,
                                            CmdLineOptions& options)
    {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
        if (vm.count("help"))
        {
            std::cout << desc << std::endl;
            return true;
        }
        if (!options.parse(vm))
        {
            std::cout << "Could not parse command line options" << std::endl;
            return true;
        }
        return false;
    }

    bool workingDirSet = false;
    bool configDirSet = false;
    bool modDirSet = false;
    std::string moddir;
    std::string configdir;
    std::string workingdir;
    std::string bindir;
};


class CmdLinOptionsServer : public CmdLineOptions
{
  public:
    CmdLinOptionsServer() {}
    bool parse(po::variables_map& vm)
    {
        if (!CmdLineOptions::parse(vm))
        {
            return false;
        }
        if (vm.count("savedir"))
        {
            savedir = vm["savedir"].as<std::string>();
            if (!workingDirSet)
            {
                workingdir = savedir;
            }
            if (!configDirSet)
            {
                configdir = savedir;
            }
        }
        else
        {
            savedir = ".";
        }
        return true;
    }
    static void createCmdLineOptions(po::options_description& desc)
    {
        CmdLineOptions::createCmdLineOptions(desc);
        desc.add_options()(
            "savedir,s", po::value<std::string>(), "Save directory");
    }
    static bool handleDefaultCmdLineOptions(int argc,
                                            char* argv[],
                                            po::options_description& desc,
                                            po::variables_map& vm,
                                            CmdLinOptionsServer& options)
    {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
        if (vm.count("help"))
        {
            std::cout << desc << std::endl;
            return true;
        }
        if (!options.parse(vm))
        {
            std::cout << "Could not parse command line options" << std::endl;
            return true;
        }
        return false;
    }

    std::string savedir;
};


class CmdLinOptionsClient : public CmdLineOptions
{
  public:
    CmdLinOptionsClient() {}
    bool parse(po::variables_map& vm)
    {
        if (!CmdLineOptions::parse(vm))
        {
            return false;
        }
        return true;
    }
    static void createCmdLineOptions(po::options_description& desc)
    {
        CmdLineOptions::createCmdLineOptions(desc);
    }
    static bool handleDefaultCmdLineOptions(int argc,
                                            char* argv[],
                                            po::options_description& desc,
                                            po::variables_map& vm,
                                            CmdLineOptions& options)
    {
        if (CmdLineOptions::handleDefaultCmdLineOptions(
                argc, argv, desc, vm, options))
        {
            return true;
        }
        return false;
    }
};

}  // namespace sphy

#endif