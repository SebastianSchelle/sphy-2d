#ifndef MAIN_WINDOW_HPP
#define MAIN_WINDOW_HPP

#include "std-inc.hpp"
#include <GLFW/glfw3.h>
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <client.hpp>
#include <sphy-2d.hpp>
#include <render/render-engine.hpp>
#include <ui/rmlui-renderinterface.hpp>
#include <config-manager/config-manager.hpp>
#include <render/texture.hpp>

namespace ui
{


struct MouseState
{
    glm::vec2 mousePos;
    int mz = 0;                                   // scroll delta
    bool buttons[3] = {false, false, false};      // left, right, middle
    bool lastButtons[3] = {false, false, false};  // left, right, middle
    tim::Timepoint timePressed[3] = {tim::getCurrentTimeU(), tim::getCurrentTimeU(), tim::getCurrentTimeU()};
    tim::Timepoint lastSingleClick[3] = {tim::getCurrentTimeU(), tim::getCurrentTimeU(), tim::getCurrentTimeU()};
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
    int width;
    int height;
};

class MainWindow
{
  public:
    MainWindow();
    ~MainWindow();
    void init();
    bool createWindow();
    bool setupRenderEngine();
    bool setupRmlUi();
    void winLoop();

  protected:
    sphyc::Client client;

  private:
    static void errorCallback(int error, const char* description);
    void onKey(int key, int scancode, int action, int mods);
    static void keyCallback(GLFWwindow* window,
                            int key,
                            int scancode,
                            int action,
                            int mods);
    void processMouseState();
    void handleWinResize();

    GLFWwindow* window;
    WindowInfo wInfo;
    const bgfx::ViewId kClearView = 0;
    MouseState mouseState;
    gfx::RenderEngine renderEngine;
    gfx::RmlUiRenderInterface rmlUiRenderInterface;
    Rml::Context* rmlContext;
    cfg::ConfigManager config;

};

}  // namespace sphyc

#endif