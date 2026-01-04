

#include "RmlUi/Core/Core.h"
#include "bgfx/defines.h"
#include "shader.hpp"
#include "vertex-defines.hpp"
#include <bgfx/platform.h>
#include <bx/bx.h>
#include <main-window.hpp>

#if BX_PLATFORM_LINUX
#define GLFW_EXPOSE_NATIVE_X11
#elif BX_PLATFORM_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#elif BX_PLATFORM_OSX
#define GLFW_EXPOSE_NATIVE_COCOA
#endif
#include "imgui/imgui.h"
#include <GLFW/glfw3native.h>


namespace ui
{


void MouseState::processMouseButton(uint8_t i)
{
    buttonReleased[i] = false;
    singleClick[i] = false;
    doubleClick[i] = false;
    longClick[i] = false;
    buttonPressed[i] = false;
    hold[i] = false;

    if (buttons[i] && !lastButtons[i])
    {
        timePressed[i] = tim::getCurrentTimeU();
        buttonPressed[i] = true;
        long clickDelta = tim::durationU(lastSingleClick[i], timePressed[i]);
        if (clickDelta < 300000U)
        {
            doubleClick[i] = true;
            // LG_D("double click");
        }
        else
        {
            singleClick[i] = true;
            // LG_D("single click");
        }
    }
    else if (!buttons[i] && lastButtons[i])
    {
        auto now = tim::getCurrentTimeU();
        buttonReleased[i] = true;
        long clickDuration = tim::durationU(timePressed[i], now);
        if (clickDuration > 300000U)
        {
            longClick[i] = true;
            // LG_D("button {} long click", i);
        }
        else
        {
            singleClick[i] = true;
            lastSingleClick[i] = now;
            // LG_D("button {} short click", i);
        }
    }
}

MainWindow::MainWindow()
    : config("modules/core/defs/client.yaml"), renderEngine(config),
      rmlUiRenderInterface(&renderEngine), client(config)
{
    uint8_t logLevel =
        static_cast<uint8_t>(std::get<float>(config.get({"loglevel"})));
    debug::createLogger("logs/logClient.txt", logLevel);
    glfwSetErrorCallback(errorCallback);
}

MainWindow::~MainWindow() {}

void MainWindow::init()
{
    client.startClient();

    if (!glfwInit())
    {
        LG_E("GLFW initialization failed");
        return;
    }

    if (!createWindow())
    {
        LG_E("Could not create GLFW window");
        return;
    }

    if (!setupRenderEngine())
    {
        LG_E("Could not setup render engine");
        return;
    }

    if (!setupRmlUi())
    {
        LG_E("Could not setup RmlUi");
        return;
    }
}

bool MainWindow::createWindow()
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(800, 600, "window", nullptr, nullptr);
    if (!window)
    {
        LG_E("Could not create GLFW window");
        return false;
    }
    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, keyCallback);

    // Call bgfx::renderFrame before bgfx::init to signal to bgfx not to create
    // a render thread. Most graphics APIs must be used on the same thread that
    // created the window.
    bgfx::renderFrame();

    // Initialize bgfx using the native window handle and window resolution.
    bgfx::Init init;
    init.type = bgfx::RendererType::Vulkan;
    init.debug = true;
#if BX_PLATFORM_LINUX || BX_PLATFORM_BSD
    init.platformData.ndt = glfwGetX11Display();
    init.platformData.nwh = (void*)(uintptr_t)glfwGetX11Window(window);
#elif BX_PLATFORM_OSX
    init.platformData.nwh = glfwGetCocoaWindow(window);
#elif BX_PLATFORM_WINDOWS
    init.platformData.nwh = glfwGetWin32Window(window);
#endif
    glfwGetWindowSize(window, &wInfo.width, &wInfo.height);
    init.resolution.width = (uint32_t)wInfo.width;
    init.resolution.height = (uint32_t)wInfo.height;
    init.resolution.reset = BGFX_RESET_VSYNC;
    if (!bgfx::init(init))
    {
        LG_E("bgfx initialization failed");
        return false;
    }

    // Set view 0 to the same dimensions as the window and to clear the color
    // buffer. const bgfx::ViewId kClearView = 0;
    bgfx::setViewRect(kClearView, 0, 0, bgfx::BackbufferRatio::Equal);

    glfwSetScrollCallback(window,
                          [](GLFWwindow* win, double xoffset, double yoffset)
                          {
                              MainWindow* self = static_cast<MainWindow*>(
                                  glfwGetWindowUserPointer(win));
                              if (self)
                              {
                                  self->mouseState.mz =
                                      static_cast<int>(yoffset);
                              }
                          });

    imguiCreate();

    return true;
}

bool MainWindow::setupRenderEngine()
{
    renderEngine.init();
    renderEngine.setWindowSize(wInfo.width, wInfo.height);
    return true;
}

bool MainWindow::setupRmlUi()
{
    // Initialize RmlUI first
    if (!Rml::Initialise())
    {
        LG_E("RmlUI initialization failed");
        return false;
    }

    // Set render interface before creating context
    Rml::SetRenderInterface(&rmlUiRenderInterface);

    // Create context (this will use the render interface)
    rmlContext =
        Rml::CreateContext("default", Rml::Vector2i(wInfo.width, wInfo.height));
    if (!rmlContext)
    {
        LG_E("Failed to create RmlUI context");
        return false;
    }

    if (!Rml::LoadFontFace("modules/core/assets/fonts/Orbitron-Regular.ttf"))
    {
        LG_E("Could not load font Orbitron-Regular.ttf");
        return false;
    }

    Rml::ElementDocument* document =
        rmlContext->LoadDocument("modules/core/assets/ui/test.rml");
    if (document)
    {
        LG_I("Loaded document test.rml");
        document->Show();
    }
    return true;
}

void MainWindow::winLoop()
{
    gfx::VertexPosColTex vertexData[3] = {
        {0.0f, 0.0f, 0x110000ff, 0.0f, 0.0f},
        {500.0f, 200.0f, 0xff00ff00, 1.0f, 0.0f},
        {200.0f, 500.0f, 0xffff0000, 0.0f, 1.0f},
    };
    uint16_t indexData[3] = {0, 1, 2};

    uint32_t geometryHandle =
        renderEngine.compileGeometry(vertexData,
                                     3 * sizeof(gfx::VertexPosColTex),
                                     indexData,
                                     3 * sizeof(uint16_t),
                                     gfx::VertexPosColTex::ms_decl);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        processMouseState();
        handleWinResize();

        rmlContext->Update();

        bool showStats = false;
        bgfx::setDebug(showStats ? BGFX_DEBUG_STATS | BGFX_DEBUG_TEXT : 0);
        bgfx::touch(0);

        bgfx::setViewClear(kClearView,
                           BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                           0x303030ff,
                           1.0f,
                           0);

        rmlContext->Render();
        rmlUiRenderInterface.EnableScissorRegion(false);
        renderEngine.renderCompiledGeometry(
            geometryHandle, glm::vec2(0.0f, 0.0f), 0, kClearView);
        bgfx::frame();
    }
}

void MainWindow::processMouseState()
{
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    mouseState.mousePos = glm::vec2(mx, my);

    mouseState.buttons[0] =
        glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    mouseState.buttons[1] =
        glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    mouseState.buttons[2] =
        glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;

    mouseState.mousePosRel.x =
        (float)mouseState.mousePos.x / (float)wInfo.width - 0.5f;  // 0.0 to 1.0
    mouseState.mousePosRel.y =
        (float)mouseState.mousePos.y / (float)wInfo.height
        - 0.5f;  // 0.0 to 1.0

    mouseState.processMouseButton(0);
    mouseState.processMouseButton(1);
    mouseState.processMouseButton(2);

    mouseState.lastButtons[0] = mouseState.buttons[0];
    mouseState.lastButtons[1] = mouseState.buttons[1];
    mouseState.lastButtons[2] = mouseState.buttons[2];
}

void MainWindow::handleWinResize()
{
    int oldWidth = wInfo.width, oldHeight = wInfo.height;
    glfwGetWindowSize(window, &wInfo.width, &wInfo.height);
    if (wInfo.width != oldWidth || wInfo.height != oldHeight)
    {
        bgfx::reset(
            (uint32_t)wInfo.width, (uint32_t)wInfo.height, BGFX_RESET_VSYNC);
        bgfx::setViewRect(kClearView, 0, 0, bgfx::BackbufferRatio::Equal);
        LG_I("Resize window to ({}, {})", wInfo.width, wInfo.height)
        renderEngine.setWindowSize(wInfo.width, wInfo.height);
    }
}

void MainWindow::errorCallback(int error, const char* description)
{
    LG_E("GLFW error {}: {}", error, description);
}

void MainWindow::keyCallback(GLFWwindow* window,
                             int key,
                             int scancode,
                             int action,
                             int mods)
{
    // Retrieve the user pointer and call the instance method
    MainWindow* self =
        static_cast<MainWindow*>(glfwGetWindowUserPointer(window));
    if (self)
    {
        self->onKey(key, scancode, action, mods);
    }
}

void MainWindow::onKey(int key, int scancode, int action, int mods) {}

}  // namespace ui