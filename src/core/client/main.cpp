#include <cmd-options.hpp>
#include <main-window.hpp>

namespace po = boost::program_options;

int main(int argc, char* argv[])
{
    po::variables_map vm;
    po::options_description desc("Allowed options");
    sphy::CmdLinOptionsClient options;
    sphy::CmdLinOptionsClient::createCmdLineOptions(desc);
    if (sphy::CmdLinOptionsClient::handleDefaultCmdLineOptions(
            argc, argv, desc, vm, options))
    {
        return 0;
    }
    ui::MainWindow mainWindow(options);
    mainWindow.initPre();
    mainWindow.winLoop();
    return 0;
}
