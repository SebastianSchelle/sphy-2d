#ifndef MAIN_WINDOW_HPP
#define MAIN_WINDOW_HPP

#include "std-inc.hpp"
#include <GLFW/glfw3.h>
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <client.hpp>
#include <cmd-options.hpp>
#include <config-manager/config-manager.hpp>
#include <lua-interpreter.hpp>
#include <mod-manager.hpp>
#include <render/render-engine.hpp>
#include <render/texture.hpp>
#include <ui/rmlui-renderinterface.hpp>
#include <ui/rmlui-systeminterface.hpp>
#include <ui/user-interface.hpp>
#include <asset-factory.hpp>

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
    uint32_t mouseZoneX;
    uint32_t mouseZoneY;
    glm::vec2 mouseWorldZonePos;
    void processMouseButton(uint8_t i);
};

struct WindowInfo
{
    glm::ivec2 size;
};

struct MenuConnectData
{
  std::string token = "1234abcd1234abcd";
  std::string ipAddress = "127.0.0.1";
  int udpPortServ = 29201;
  int tcpPortServ = 29200;
  int udpPortCli = 29202;
};

struct MenuData
{
    vector<mod::MenuDataMod> mods;
    MenuConnectData connectData;
};

class MainWindow
{
  public:
    enum class State
    {
        Init,
        LoadingMods,
        MainMenu,
        Something,
    };

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

    void setupDataModelDebug();
    void setupDataModelMenu();

    void stopServer();

    void onNewGame(Rml::DataModelHandle handle,
                   Rml::Event& event,
                   const Rml::VariantList& args);

    void onConnectToServer(Rml::DataModelHandle handle,
                         Rml::Event& event,
                         const Rml::VariantList& args);

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

    State state = State::Init;

    tim::Timepoint lastLoopTime;
    float frameTimeFiltered = 0.0f;
    uint16_t fps;

    UiDocHandle modLoadingHandle;

    std::thread loadingThread;
    std::future<bool> loadingFuture;

    Rml::DataModelHandle rmlModelDebug;
    Rml::DataModelHandle rmlModelMenu;
    Rml::DataModelHandle rmlModelHud;

    MenuData menuData;

    std::string debugInput = "Hello World!";
    bp::child *serverProcess = nullptr;
    ecs::AssetFactory assetFactory;
};

}  // namespace ui

#endif