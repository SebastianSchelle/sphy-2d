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

MainWindow::MainWindow(def::CmdLinOptionsClient& options)
    : options(options),
      config(options.workingdir + "/modules/core/defs/client.yaml"),
      renderEngine(config), rmlUiRenderInterface(&renderEngine), client(config),
      modManager(), modLoadingHandle(UiDocHandle::Invalid())
{
    auto path(options.workingdir);
    std::filesystem::current_path(path);
    uint8_t logLevel =
        static_cast<uint8_t>(std::get<float>(config.get({"loglevel"})));
    debug::createLogger("logs/logClient.txt", logLevel);
    glfwSetErrorCallback(errorCallback);
}

MainWindow::~MainWindow()
{
    // Wait for loading thread to finish before destroying resources
    if (loadingThread.joinable())
    {
        loadingThread.join();
    }
    renderEngine.~RenderEngine();
    Rml::Shutdown();
    bgfx::shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
}

bool MainWindow::initPre()
{
    client.startClient();

    if (!glfwInit())
    {
        LG_E("GLFW initialization failed");
        return false;
    }

    if (!createWindow())
    {
        LG_E("Could not create GLFW window");
        return false;
    }

    if (!renderEngine.initPre())
    {
        LG_E("Failed to initialize render engine");
        return false;
    }
    renderEngine.setWindowSize(wInfo.size.x, wInfo.size.y);

    if (!Rml::Initialise())
    {
        LG_E("RmlUI initialization failed");
        return false;
    }

    // Set render interface before creating context
    Rml::SetRenderInterface(&rmlUiRenderInterface);
    if (!userInterface.init(wInfo.size))
    {
        LG_E("Could not initialize user interface");
        return false;
    }
    return true;
}

bool MainWindow::initPost()
{
    return true;
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
    glfwGetWindowSize(window, &wInfo.size.x, &wInfo.size.y);
    init.resolution.width = (uint32_t)wInfo.size.x;
    init.resolution.height = (uint32_t)wInfo.size.y;
    init.resolution.reset = BGFX_RESET_VSYNC;
    if (!bgfx::init(init))
    {
        LG_E("bgfx initialization failed");
        return false;
    }

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

    return true;
}

void MainWindow::winLoop()
{
    gfx::VertexPosColTex vertexData[] = {
        {0.0f, 0.0f, 0xffffffff, 0.0f, 0.0f},
        {0.0f, 400.0f, 0xffffffff, 0.0f, 150.0f},
        {400.0f, 0.0f, 0xffffffff, 145.0f, 0.0f},
        {400.0f, 400.0f, 0xffffffff, 145.0f, 150.0f},
    };
    uint16_t indexData[] = {0, 1, 2, 2, 1, 3};

    gfx::GeometryHandle geometryHandle =
        renderEngine.compileGeometry(vertexData,
                                     4 * sizeof(gfx::VertexPosColTex),
                                     indexData,
                                     6 * sizeof(uint16_t),
                                     gfx::VertexPosColTex::ms_decl);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        processMouseState();
        handleWinResize();

        for (int i = 0; i < 3; ++i)
        {
            if (mouseState.buttonPressed[i])
            {
                userInterface.processMouseButtonDown(i, 0);
            }
            if (mouseState.buttonReleased[i])
            {
                userInterface.processMouseButtonUp(i, 0);
            }
        }
        userInterface.processMouseWheel(mouseState.mz, 0);
        userInterface.update();

        renderEngine.startFrame();

        switch (state)
        {
            case State::Init:
                startLoading();
                break;
            case State::LoadingMods:
                loadingLoop();
                break;
            case State::Something:
                break;
        }

        // Draw star background
        renderEngine.drawFullScreenTriangles(
            0, renderEngine.getShaderHandle("distantstars"));

        userInterface.render();
        //  rmlUiRenderInterface.EnableScissorRegion(false);
        //  gfx::TextureHandle textureHandle = gfx::TextureHandle::Invalid();
        //  renderEngine.renderCompiledGeometry(
        //      gfx::GeometryHandle(0, 1), glm::vec2(200.0f, 150.0f),
        //      gfx::TextureHandle(2, 1), kClearView);
        bgfx::frame();
    }
}

void MainWindow::startLoading()
{
    // Prevent multiple calls
    if (state == State::LoadingMods || loadingThread.joinable())
    {
        LG_W("Loading already started, ignoring duplicate call");
        return;
    }

    userInterface.showDocument(userInterface.getDocumentHandle("mod-loading"));

    LG_I("Start loading mods...");
    std::promise<bool> loadingPromise;
    loadingFuture = loadingPromise.get_future();
    loadingThread = std::thread(
        [this](std::promise<bool> succ)
        {
            std::vector<std::string> modList;
            if (!modManager.parseModList("modules/modlist.txt", modList))
            {
                LG_E("Failed to parse mod list");
                succ.set_value(false);
                return;
            }
            if (!modManager.checkDependencies(modList, "modules"))
            {
                LG_E("Failed to check dependencies");
                succ.set_value(false);
                return;
            }
            mod::PtrHandles ptrHandles{&renderEngine};
            if (!modManager.loadMods(ptrHandles))
            {
                LG_E("Failed to load mods");
                succ.set_value(false);
                return;
            }
            succ.set_value(true);
        },
        std::move(loadingPromise));

    // Set state immediately to prevent calling startLoading() again
    state = State::LoadingMods;
}

void MainWindow::loadingLoop()
{
    if (loadingFuture.valid()
        && loadingFuture.wait_for(std::chrono::seconds(0))
               == std::future_status::ready)
    {
        userInterface.hideDocument(
            userInterface.getDocumentHandle("mod-loading"));
        if (loadingFuture.get())
        {
            LG_I("Mods loaded successfully");
            if (loadingThread.joinable())
            {
                loadingThread.join();
            }
            state = State::Something;
        }
        else
        {
            LG_E("Failed to load mods");
            exit(1);
        }
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
        (float)mouseState.mousePos.x / (float)wInfo.size.x
        - 0.5f;  // 0.0 to 1.0
    mouseState.mousePosRel.y =
        (float)mouseState.mousePos.y / (float)wInfo.size.y
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
    int oldWidth = wInfo.size.x, oldHeight = wInfo.size.y;
    glfwGetWindowSize(window, &wInfo.size.x, &wInfo.size.y);
    if (wInfo.size.x != oldWidth || wInfo.size.y != oldHeight)
    {
        bgfx::reset(
            (uint32_t)wInfo.size.x, (uint32_t)wInfo.size.y, BGFX_RESET_VSYNC);
        renderEngine.setWindowSize(wInfo.size.x, wInfo.size.y);
        userInterface.setDimensions(wInfo.size);
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