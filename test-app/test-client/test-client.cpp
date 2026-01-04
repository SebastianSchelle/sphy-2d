#include <test-client.hpp>
#include <main-window.hpp>

TestClient::TestClient() : ui::MainWindow() {
    init();
    
    winLoop();
}
TestClient::~TestClient() {}
