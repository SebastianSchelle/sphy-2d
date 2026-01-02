#include "sphy-client.hpp"

namespace sphyc {

int test(int argc, char *argv[]) {
    logging::createLogger("logs/logClient.txt");
    LG_I("Client test function");
    return 0;
}

} // namespace sphyc