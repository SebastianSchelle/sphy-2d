#include <server.hpp>
#include <boost/program_options.hpp>
#include <cmd-options.hpp>

namespace po = boost::program_options;

int main(int argc, char* argv[])
{
    po::variables_map vm;
    po::options_description desc("Allowed options");
    sphy::CmdLinOptionsServer options;
    sphy::CmdLinOptionsServer::createCmdLineOptions(desc);
    if (sphy::CmdLinOptionsServer::handleDefaultCmdLineOptions(argc, argv, desc, vm, options)) {
        return 0;
    }
    sphys::Server server(options);
    server.startServer();
    return 0;
}