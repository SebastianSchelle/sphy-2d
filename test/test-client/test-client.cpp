#include <test-client.hpp>
#include <main-window.hpp>

TestClient::TestClient() : ui::MainWindow() {
    std::cout << "cwd: " << std::filesystem::current_path() << "\n";
    init();
    winLoop();
}
TestClient::~TestClient() {}
