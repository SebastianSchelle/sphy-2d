#ifndef CMD_OPTIONS_HPP
#define CMD_OPTIONS_HPP

#include <boost/program_options.hpp>
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
        if (vm.count("workingdir"))
        {
            workingdir = vm["workingdir"].as<std::string>();
        }
        else
        {
            workingdir = ".";
        }
        if (vm.count("savedir"))
        {
            savedir = vm["savedir"].as<std::string>();
        }
        else
        {
            savedir = ".";
        }
        return true;
    }

    static void createCmdLineOptions(po::options_description& desc)
    {
        desc.add_options()("help,h", "Print help message")(
            "workingdir,w", po::value<std::string>(), "Working directory")(
            "savedir,s", po::value<std::string>(), "Save directory");
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

    std::string workingdir;
    std::string savedir;
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
        if (vm.count("rerun"))
        {
            enableRerun = vm["rerun"].as<bool>();
        }
        return true;
    }
    static void createCmdLineOptions(po::options_description& desc)
    {
        CmdLineOptions::createCmdLineOptions(desc);
        desc.add_options()(
            "rerun,R",
            po::bool_switch()->default_value(false),
            "Enable rerun streaming from server");
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

    bool enableRerun = false;
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
        if (vm.count("bgfx-debug"))
        {
            bgfxDebug = vm["bgfx-debug"].as<bool>();
        }
        return true;
    }
    static void createCmdLineOptions(po::options_description& desc)
    {
        CmdLineOptions::createCmdLineOptions(desc);
        desc.add_options()(
            "bgfx-debug",
            po::bool_switch()->default_value(false),
            "Enable bgfx debug mode");
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

    bool bgfxDebug = false;
};

}  // namespace sphy

#endif