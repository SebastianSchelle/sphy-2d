#ifndef MAIN_WINDOW_HPP
#define MAIN_WINDOW_HPP

#include "std-inc.hpp"
#include <GLFW/glfw3.h>
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <client.hpp>
#include <cmd-options.hpp>
#include <config-manager/config-manager.hpp>
#include <functional>
#include <future>
#include <lua-interpreter.hpp>
#include <mod-manager.hpp>
#include <model.hpp>
#include <mutex>
#include <render/render-engine.hpp>
#include <render/texture.hpp>
#include <ui/rmlui-renderinterface.hpp>
#include <ui/rmlui-systeminterface.hpp>
#include <ui/user-interface.hpp>
#include <vector>
#include <version.hpp>

namespace ui
{


struct MouseState
{
    glm::vec2 mousePos;
    int mz = 0;                                   // scroll delta
    bool buttons[3] = {false, false, false};      // left, right, middle
    bool lastButtons[3] = {false, false, false};  // left, right, middle
    tim::Timepoint timePressed[3] = {tim::getCurrentTimeU(),
                                     tim::getCurrentTimeU(),
                                     tim::getCurrentTimeU()};
    tim::Timepoint lastSingleClick[3] = {tim::getCurrentTimeU(),
                                         tim::getCurrentTimeU(),
                                         tim::getCurrentTimeU()};
    bool buttonPressed[3] = {false, false, false};
    bool buttonReleased[3] = {false, false, false};
    bool singleClick[3] = {false, false, false};
    bool doubleClick[3] = {false, false, false};
    bool longClick[3] = {false, false, false};
    bool hold[3] = {false, false, false};
    glm::vec2 mousePosRel;
    def::SectorCoords mouseSectorCoords;
    void processMouseButton(uint8_t i);
};

struct WindowInfo
{
    glm::ivec2 size;
};

struct UiMenuConnectData
{
    std::string token = "1234abcd1234abcd";
    std::string ipAddress = "127.0.0.1";
    int udpPortServ = 29201;
    int tcpPortServ = 29200;
    int udpPortCli = 29202;
};

struct UiMenuData
{
    vector<mod::MenuDataMod> mods;
    UiMenuConnectData connectData;
    bool inGame = false;
};

struct UiDbgInputData
{
    float ptrScreenX = 0.f;
    float ptrScreenY = 0.f;
    bool ptrOverUi = false;
    uint32_t ptrSectorX = 0;
    uint32_t ptrSectorY = 0;
    float ptrSectorPosX = 0.f;
    float ptrSectorPosY = 0.f;
};

struct UiDbgViewData
{
    int winW = 0;
    int winH = 0;
    float fpsSmoothed = 0.f;
    float frameMs = 0.f;
    float zoom = 0.f;
    float camX = 0.f;
    float camY = 0.f;
    int32_t sectorOffsetX = 0;
    int32_t sectorOffsetY = 0;
};

struct UiDbgGameData
{
    std::string clientVersion = version::VERSION_STRING;
    std::string serverVersion = "Unknown";
    std::string gameState = "Init";
};

struct UiDbgConnectionData
{
    float serverLatency = 0.0f;
    float serverTimeOffset = 0.0f;
};

struct UiDebugData
{
    UiDbgInputData inputData;
    UiDbgViewData viewData;
    UiDbgGameData gameData;
    UiDbgConnectionData connectionData;
};

class MainWindow
{
  public:
    MainWindow(sphy::CmdLinOptionsClient& options);
    ~MainWindow();
    bool initPre();
    bool initPost();
    bool createWindow();
    bool setupRenderEngine();
    bool setupRmlUi();
    void winLoop();

  protected:
    sphyc::Client client;

  private:
    static void errorCallback(int error, const char* description);
    void onKey(int key, int scancode, int action, int mods);
    void onChar(unsigned int codepoint);
    void onScroll(double xoffset, double yoffset);
    static void keyCallback(GLFWwindow* window,
                            int key,
                            int scancode,
                            int action,
                            int mods);

    static void charCallback(GLFWwindow* window, unsigned int codepoint);
    static void
    scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    void processMouseState();
    void handleWinResize();

    void startLoading();
    void loadingLoop();
    void processUiTasks();
    void drainUiTasksForShutdown();

    void setupDataModelDebug();
    void updateDebugDataModel(float deltaTimeSec, bool ptrOverUi);
    void setupDataModelMenu();
    void updateMenuDataModel();

    void stopServer();

    void onNewGame(Rml::DataModelHandle handle,
                   Rml::Event& event,
                   const Rml::VariantList& args);

    void onConnectToServer(Rml::DataModelHandle handle,
                           Rml::Event& event,
                           const Rml::VariantList& args);
    void onCmd(const std::string& cmd);

    void onClientShutdown();

    void onQuit(Rml::DataModelHandle handle,
                Rml::Event& event,
                const Rml::VariantList& args);

    void onExitToMenu(Rml::DataModelHandle handle,
                      Rml::Event& event,
                      const Rml::VariantList& args);

    void onAfterLoadWorld();

    static Rml::Input::KeyIdentifier glfwToRmlKey(int key);

    GLFWwindow* window;
    WindowInfo wInfo;
    MouseState mouseState;
    gfx::RenderEngine renderEngine;
    gfx::RmlUiRenderInterface rmlUiRenderInterface;
    ui::RmlUiSystemInterface rmlUiSystemInterface;
    cfg::ConfigManager config;
    sphy::CmdLinOptionsClient options;
    mod::ModManager modManager;
    ui::UserInterface userInterface;
    mod::LuaInterpreter luaInterpreter;
    sphyc::Model model;

    tim::Timepoint lastLoopTime;
    float frameTimeFiltered = 0.0f;

    UiDocHandle modLoadingHandle;

    std::thread loadingThread;
    std::future<bool> loadingFuture;

    std::mutex uiTaskMutex;
    std::vector<std::function<void()>> uiTasks;

    Rml::DataModelHandle rmlModelDebug;
    Rml::DataModelHandle rmlModelMenu;
    Rml::DataModelHandle rmlModelHud;

    UiMenuData menuData;
    UiDebugData debugData;

    bp::child* serverProcess = nullptr;

    gfx::PanDirection panX = gfx::PanDirection::Stop;
    gfx::PanDirection panY = gfx::PanDirection::Stop;
};

}  // namespace ui

#endif