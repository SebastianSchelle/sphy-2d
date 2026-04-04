#include "GLFW/glfw3.h"
#include "RmlUi/Core/Core.h"
#include "bgfx/defines.h"
#include "vertex-defines.hpp"
#include <bgfx/platform.h>
#include <bx/bx.h>
#include <comp-ident.hpp>
#include <main-window.hpp>
#include <memory>
#include <os-helper.hpp>
#include <sol/sol.hpp>

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

MainWindow::MainWindow(sphy::CmdLinOptionsClient& options)
    : options(options),
      config(options.workingdir + "/modules/core/defs/client.yaml"),
      renderEngine(config), rmlUiRenderInterface(&renderEngine),
      client(config, model.sendQueue, model.receiveQueue), modManager(),
      modLoadingHandle(UiDocHandle::Invalid()),
      userInterface(std::bind(&MainWindow::onCmd, this, std::placeholders::_1)),
      model(&userInterface, std::bind(&MainWindow::onAfterLoadWorld, this))
{
    auto path(options.workingdir);
    std::filesystem::current_path(path);
    uint8_t logLevel =
        static_cast<uint8_t>(std::get<float>(config.get({"loglevel"})));
    debug::createLogger("logs/logClient.txt", logLevel);
    glfwSetErrorCallback(errorCallback);
    client.setShutdownCallback(
        [this]()
        {
            std::lock_guard<std::mutex> lock(uiTaskMutex);
            uiTasks.push_back([this]() { onClientShutdown(); });
        });
}

MainWindow::~MainWindow()
{
    // Wait for loading thread to finish before destroying resources
    if (loadingThread.joinable())
    {
        drainUiTasksForShutdown();
        loadingThread.join();
    }
    stopServer();
    renderEngine.shutdown();
    Rml::Shutdown();
    bgfx::shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
}

bool MainWindow::initPre()
{
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
    Rml::SetSystemInterface(&rmlUiSystemInterface);
    Rml::SetRenderInterface(&rmlUiRenderInterface);
    if (!userInterface.init(wInfo.size))
    {
        LG_E("Could not initialize user interface");
        return false;
    }

    setupDataModelDebug();
    setupDataModelMenu();

    return true;
}

bool MainWindow::initPost()
{
    return true;
}

bool MainWindow::createWindow()
{
    uint32_t wWidth = CFG_UINT(config, "win", "width");
    uint32_t wHeight = CFG_UINT(config, "win", "height");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(wWidth, wHeight, "window", nullptr, nullptr);
    if (!window)
    {
        LG_E("Could not create GLFW window");
        return false;
    }
    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCharCallback(window, charCallback);

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

    glfwSetScrollCallback(window, scrollCallback);

    return true;
}

void MainWindow::winLoop()
{
    lastLoopTime = tim::getCurrentTimeU();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    while (!glfwWindowShouldClose(window))
    {
        processUiTasks();

        // luaInterpreter.callFunction("dbg.test");

        tim::Timepoint now = tim::getCurrentTimeU();
        float deltaTime = (float)tim::durationU(lastLoopTime, now) / 1000000.0f;
        lastLoopTime = now;
        frameTimeFiltered = 0.9f * frameTimeFiltered + 0.1f * deltaTime;
        debugData.viewData.fpsSmoothed =
            frameTimeFiltered > 1e-6f ? 1.0f / frameTimeFiltered : 0.f;

        model.modelLoop(deltaTime);

        mouseState.mz = 0;
        glfwPollEvents();
        processMouseState();
        handleWinResize();

        renderEngine.screenToSectorCoords(mouseState.mousePos, mouseState.mouseSectorCoords);

        const bool mouseOverUi =
            userInterface.processMouseMove(mouseState.mousePos, 0);
        debugData.inputData.ptrOverUi = mouseOverUi;
        bool mouseWheelInteract =
            userInterface.processMouseWheel(mouseState.mz, 0);
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

        if (!mouseOverUi && !mouseWheelInteract)
        {
            if (mouseState.mz != 0)
            {
                glm::vec2 mousePosWorldBefore =
                    renderEngine.screenToWorldPixel(mouseState.mousePos);
                renderEngine.zoomWorld(mouseState.mz);
                renderEngine.updateWorldView();
                glm::vec2 mousePosWorldAfter =
                    renderEngine.screenToWorldPixel(mouseState.mousePos);
                renderEngine.panWorld(mousePosWorldBefore - mousePosWorldAfter);
            }
        }

        if (userInterface.isDebugOpen())
        {
            updateDebugDataModel(deltaTime, mouseOverUi);
            rmlModelDebug.DirtyAllVariables();
        }

        if (userInterface.isMenuOpen())
        {
            updateMenuDataModel();
            rmlModelMenu.DirtyAllVariables();
        }

        userInterface.update();

        switch (model.getGameState())
        {
            case ClientGameState::Init:
                startLoading();
                processUiTasks();
                break;
            case ClientGameState::LoadingMods:
                loadingLoop();
                break;
            case ClientGameState::MainMenu:
                userInterface.showMenu();
                break;
            case ClientGameState::Authenticated:
                break;
            case ClientGameState::LoadWorld:
                break;
            case ClientGameState::GameLoop:
                break;
            default:
                break;
        }

        float t =
            tim::durationU(renderEngine.getStartTime(), tim::getCurrentTimeU())
            / 1000000.0f;


        renderEngine.startFrame();

        // renderEngine.setWorldCameraPosition(
        //     glm::vec2(200.0f + 50.0f * sin(t), 60.0f + 50.0f * cos(t
        //     * 1.5f)));

        renderEngine.drawFullScreenTriangles(
            0, renderEngine.getShaderHandle("distantstars"));

        if (model.getGameState() == ClientGameState::GameLoop)
        {
            float zoom = renderEngine.getWorldZoom();

            renderEngine.panWorld(panX, panY);


            // renderEngine.drawRectangle(glm::vec2(0.0f, 0.0f),
            //                            glm::vec2(1000000.0f, 1.0f/zoom),
            //                            0x10ffffff,
            //                            1.0f/zoom);
            // renderEngine.drawRectangle(glm::vec2(0.0f, 0.0f),
            //                            glm::vec2(1.0f/zoom, 1000000.0f),
            //                            0x10ffffff,
            //                            1.0f/zoom);

            // renderEngine.drawRectangle(glm::vec2(200.0f + 50.0f * sin(t),
            //                                      60.0f + 50.0f * cos(t * 1.5f)),
            //                            glm::vec2(10.0f, 10.0f),
            //                            0xffffffff,
            //                            4.0f,
            //                            -t * 50.0f,
            //                            0);
            // renderEngine.drawRectangle(glm::vec2(200.0f + 50.0f * sin(t),
            //                                      60.0f + 50.0f * cos(t * 1.5f)),
            //                            glm::vec2(200.0f, 200.0f),
            //                            0xff00ff10,
            //                            2.0f,
            //                            t * 4.0f,
            //                            0);
            // renderEngine.drawEllipse(glm::vec2(160.0f, 260.0f),
            //                          glm::vec2(50.0f, 100.0f),
            //                          0xffffffff,
            //                          1.0f,
            //                          t,
            //                          0);
            // renderEngine.drawEllipse(glm::vec2(160.0f, 260.0f),
            //                          glm::vec2(60.0f + 50.0f * sin(t),
            //                                    60.0f + 50.0f * cos(t * 1.5f)),
            //                          0xab0112ff,
            //                          1.0f,
            //                          0,
            //                          0);
            model.drawDebug(renderEngine, zoom);
        }

        userInterface.render();
        renderEngine.endFrame();
    }
}

void MainWindow::processUiTasks()
{
    std::vector<std::function<void()>> batch;
    {
        std::lock_guard<std::mutex> lock(uiTaskMutex);
        batch.swap(uiTasks);
    }
    for (auto& task : batch)
    {
        task();
    }
}

void MainWindow::drainUiTasksForShutdown()
{
    for (;;)
    {
        std::vector<std::function<void()>> batch;
        {
            std::lock_guard<std::mutex> lock(uiTaskMutex);
            batch.swap(uiTasks);
        }
        if (batch.empty())
        {
            return;
        }
        for (auto& task : batch)
        {
            task();
        }
    }
}

void MainWindow::startLoading()
{
    // Prevent multiple calls
    if (model.getGameState() != ClientGameState::Init
        || loadingThread.joinable())
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
            mod::PtrHandles ptrHandles{
                .renderEngine = &renderEngine,
                .userInterface = &userInterface,
                .runUiBool = [this](std::function<bool()> fn) -> bool
                {
                    auto p = std::make_shared<std::promise<bool>>();
                    std::future<bool> fut = p->get_future();
                    {
                        std::lock_guard<std::mutex> lock(uiTaskMutex);
                        uiTasks.push_back(
                            [fn = std::move(fn), p]()
                            {
                                try
                                {
                                    p->set_value(fn());
                                }
                                catch (...)
                                {
                                    p->set_exception(std::current_exception());
                                }
                            });
                    }
                    try
                    {
                        return fut.get();
                    }
                    catch (...)
                    {
                        return false;
                    }
                },
                .luaInterpreter = &luaInterpreter,
                .assetFactory = model.getAssetFactory(),
            };
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
    model.startLoadingMods();
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
            modManager.populateMenuData(menuData.mods);
            rmlModelMenu.DirtyVariable("mods");
            userInterface.showMenu();
            luaInterpreter.dumpAllTables();
            model.startModel();
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

void MainWindow::onKey(int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS && key == GLFW_KEY_ENTER
        && mods == GLFW_MOD_CONTROL)
    {
        userInterface.toggleChat();
        return;
    }

    if (action == GLFW_PRESS && key == GLFW_KEY_P && mods == GLFW_MOD_CONTROL)
    {
        userInterface.toggleDebug();
        return;
    }

    if (action == GLFW_PRESS
        && !userInterface.processKeyDown(glfwToRmlKey(key)))
    {
        return;
    }
    else if (action == GLFW_REPEAT
             && !userInterface.processKeyDown(glfwToRmlKey(key)))
    {
        return;
    }
    else if (action == GLFW_RELEASE
             && !userInterface.processKeyUp(glfwToRmlKey(key)))
    {
        return;
    }

    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE)
    {
        userInterface.processEsc(model.getGameState()
                                 == ClientGameState::MainMenu);
        return;
    }

    if (key == GLFW_KEY_W)
    {
        if (action == GLFW_PRESS)
        {
            panY = gfx::PanDirection::Up;
        }
        else if (action == GLFW_RELEASE && panY == gfx::PanDirection::Up)
        {
            panY = gfx::PanDirection::Stop;
        }
        return;
    }
    if (key == GLFW_KEY_S)
    {
        if (action == GLFW_PRESS)
        {
            panY = gfx::PanDirection::Down;
        }
        else if (action == GLFW_RELEASE && panY == gfx::PanDirection::Down)
        {
            panY = gfx::PanDirection::Stop;
        }
        return;
    }
    if (key == GLFW_KEY_A)
    {
        if (action == GLFW_PRESS)
        {
            panX = gfx::PanDirection::Left;
        }
        else if (action == GLFW_RELEASE && panX == gfx::PanDirection::Left)
        {
            panX = gfx::PanDirection::Stop;
        }
        return;
    }
    if (key == GLFW_KEY_D)
    {
        if (action == GLFW_PRESS)
        {
            panX = gfx::PanDirection::Right;
        }
        else if (action == GLFW_RELEASE && panX == gfx::PanDirection::Right)
        {
            panX = gfx::PanDirection::Stop;
        }
        return;
    }
}

void MainWindow::charCallback(GLFWwindow* window, unsigned int codepoint)
{
    MainWindow* self =
        static_cast<MainWindow*>(glfwGetWindowUserPointer(window));
    if (self)
    {
        self->onChar(codepoint);
    }
}

void MainWindow::onChar(unsigned int codepoint)
{
    userInterface.processTextInput(static_cast<Rml::Character>(codepoint));
}

void MainWindow::scrollCallback(GLFWwindow* window,
                                double xoffset,
                                double yoffset)
{
    MainWindow* self =
        static_cast<MainWindow*>(glfwGetWindowUserPointer(window));
    if (self)
    {
        self->onScroll(xoffset, yoffset);
    }
}

void MainWindow::onScroll(double xoffset, double yoffset)
{
    mouseState.mz = static_cast<int>(yoffset);
}

Rml::Input::KeyIdentifier MainWindow::glfwToRmlKey(int key)
{
    using namespace Rml::Input;

    switch (key)
    {
        case GLFW_KEY_LEFT:
            return KI_LEFT;
        case GLFW_KEY_RIGHT:
            return KI_RIGHT;
        case GLFW_KEY_UP:
            return KI_UP;
        case GLFW_KEY_DOWN:
            return KI_DOWN;

        case GLFW_KEY_BACKSPACE:
            return KI_BACK;
        case GLFW_KEY_DELETE:
            return KI_DELETE;
        case GLFW_KEY_KP_ENTER:
            return KI_NUMPADENTER;
        case GLFW_KEY_ENTER:
            return KI_RETURN;
        case GLFW_KEY_TAB:
            return KI_TAB;
        case GLFW_KEY_ESCAPE:
            return KI_ESCAPE;

        case GLFW_KEY_HOME:
            return KI_HOME;
        case GLFW_KEY_END:
            return KI_END;
        case GLFW_KEY_PAGE_UP:
            return KI_PRIOR;
        case GLFW_KEY_PAGE_DOWN:
            return KI_NEXT;
    }

    return KI_UNKNOWN;
}

static const char* gameStateToString(ClientGameState s)
{
    switch (s)
    {
        case ClientGameState::Init:
            return "Init";
        case ClientGameState::LoadingMods:
            return "LoadingMods";
        case ClientGameState::MainMenu:
            return "MainMenu";
        case ClientGameState::VersionCheck:
            return "VersionCheck";
        case ClientGameState::Authenticating:
            return "Authenticating";
        case ClientGameState::Authenticated:
            return "Authenticated";
        case ClientGameState::LoadWorld:
            return "LoadWorld";
        case ClientGameState::GameLoop:
            return "GameLoop";
    }
    return "?";
}

void MainWindow::updateMenuDataModel()
{
    menuData.inGame = model.getGameState() == ClientGameState::GameLoop;
}

void MainWindow::updateDebugDataModel(float deltaTimeSec, bool ptrOverUi)
{
    debugData.viewData.frameMs = deltaTimeSec * 1000.f;
    debugData.viewData.fpsSmoothed =
        frameTimeFiltered > 1e-6f ? 1.0f / frameTimeFiltered : 0.f;
    debugData.viewData.zoom = renderEngine.getWorldZoom();
    debugData.viewData.camX = renderEngine.getWorldCameraPosition().x;
    debugData.viewData.camY = renderEngine.getWorldCameraPosition().y;
    debugData.viewData.winW = wInfo.size.x;
    debugData.viewData.winH = wInfo.size.y;
    debugData.inputData.ptrOverUi = ptrOverUi;
    debugData.inputData.ptrScreenX = mouseState.mousePos.x;
    debugData.inputData.ptrScreenY = mouseState.mousePos.y;
    debugData.inputData.ptrSectorX = mouseState.mouseSectorCoords.sectorX;
    debugData.inputData.ptrSectorY = mouseState.mouseSectorCoords.sectorY;
    debugData.inputData.ptrSectorPosX = mouseState.mouseSectorCoords.sectorPos.x;
    debugData.inputData.ptrSectorPosY = mouseState.mouseSectorCoords.sectorPos.y;
    debugData.gameData.gameState = gameStateToString(model.getGameState());
    debugData.connectionData.serverLatency =
        model.getTimeSyncData().serverLatency;
    debugData.connectionData.serverTimeOffset =
        model.getTimeSyncData().serverOffset;
    debugData.viewData.sectorOffsetX = renderEngine.getSectorOffsetX();
    debugData.viewData.sectorOffsetY = renderEngine.getSectorOffsetY();
}

void MainWindow::setupDataModelDebug()
{
    auto debugConstructor = userInterface.getDataModel("debug");
    if (debugConstructor)
    {
        if (auto md_handle = debugConstructor.RegisterStruct<UiDbgInputData>())
        {
            md_handle.RegisterMember("ptrScreenX", &UiDbgInputData::ptrScreenX);
            md_handle.RegisterMember("ptrScreenY", &UiDbgInputData::ptrScreenY);
            md_handle.RegisterMember("ptrOverUi", &UiDbgInputData::ptrOverUi);
            md_handle.RegisterMember("ptrSectorX", &UiDbgInputData::ptrSectorX);
            md_handle.RegisterMember("ptrSectorY", &UiDbgInputData::ptrSectorY);
            md_handle.RegisterMember("ptrSectorPosX", &UiDbgInputData::ptrSectorPosX);
            md_handle.RegisterMember("ptrSectorPosY", &UiDbgInputData::ptrSectorPosY);
        }
        debugConstructor.Bind("inputData", &debugData.inputData);

        if (auto md_handle = debugConstructor.RegisterStruct<UiDbgViewData>())
        {
            md_handle.RegisterMember("winW", &UiDbgViewData::winW);
            md_handle.RegisterMember("winH", &UiDbgViewData::winH);
            md_handle.RegisterMember("fpsSmoothed",
                                     &UiDbgViewData::fpsSmoothed);
            md_handle.RegisterMember("frameMs", &UiDbgViewData::frameMs);
            md_handle.RegisterMember("zoom", &UiDbgViewData::zoom);
            md_handle.RegisterMember("camX", &UiDbgViewData::camX);
            md_handle.RegisterMember("camY", &UiDbgViewData::camY);
            md_handle.RegisterMember("sectorOffsetX", &UiDbgViewData::sectorOffsetX);
            md_handle.RegisterMember("sectorOffsetY", &UiDbgViewData::sectorOffsetY);
        }
        debugConstructor.Bind("viewData", &debugData.viewData);

        if (auto md_handle =
                debugConstructor.RegisterStruct<UiDbgConnectionData>())
        {
            md_handle.RegisterMember("serverLatency",
                                     &UiDbgConnectionData::serverLatency);
            md_handle.RegisterMember("serverTimeOffset",
                                     &UiDbgConnectionData::serverTimeOffset);
        }
        debugConstructor.Bind("connectionData", &debugData.connectionData);

        if (auto md_handle = debugConstructor.RegisterStruct<UiDbgGameData>())
        {
            md_handle.RegisterMember("gameState", &UiDbgGameData::gameState);
        }
        debugConstructor.Bind("gameData", &debugData.gameData);
        rmlModelDebug = debugConstructor.GetModelHandle();
    }
}

void MainWindow::setupDataModelMenu()
{
    auto menuConstructor = userInterface.getDataModel("menu");
    if (menuConstructor)
    {
        LG_D("Data model 'menu' created");
        menuConstructor.BindEventCallback(
            "onNavigate", &UserInterface::onMenuNavigate, &userInterface);
        menuConstructor.BindEventCallback("onQuit", &MainWindow::onQuit, this);
        menuConstructor.BindEventCallback(
            "onBack", &UserInterface::onMenuBack, &userInterface);
        menuConstructor.BindEventCallback(
            "onExitToMenu", &MainWindow::onExitToMenu, this);
        menuConstructor.BindEventCallback(
            "onNewGame", &MainWindow::onNewGame, this);
        menuConstructor.BindEventCallback(
            "connectToServer", &MainWindow::onConnectToServer, this);

        if (auto md_handle = menuConstructor.RegisterStruct<mod::MenuDataMod>())
        {
            md_handle.RegisterMember("id", &mod::MenuDataMod::id);
            md_handle.RegisterMember("name", &mod::MenuDataMod::name);
            md_handle.RegisterMember("description",
                                     &mod::MenuDataMod::description);
            md_handle.RegisterMember("hasModOptions",
                                     &mod::MenuDataMod::hasModOptions);
        }
        menuConstructor.RegisterArray<std::vector<mod::MenuDataMod>>();
        menuConstructor.Bind("mods", &menuData.mods);

        if (auto md_handle =
                menuConstructor.RegisterStruct<UiMenuConnectData>())
        {
            md_handle.RegisterMember("token", &UiMenuConnectData::token);
            md_handle.RegisterMember("ipAddress",
                                     &UiMenuConnectData::ipAddress);
            md_handle.RegisterMember("udpPortServ",
                                     &UiMenuConnectData::udpPortServ);
            md_handle.RegisterMember("tcpPortServ",
                                     &UiMenuConnectData::tcpPortServ);
            md_handle.RegisterMember("udpPortCli",
                                     &UiMenuConnectData::udpPortCli);
        }
        menuConstructor.Bind("connectData", &menuData.connectData);

        menuConstructor.Bind("inGame", &menuData.inGame);

        rmlModelMenu = menuConstructor.GetModelHandle();
    }
}

void MainWindow::onNewGame(Rml::DataModelHandle handle,
                           Rml::Event& event,
                           const Rml::VariantList& args)
{
    auto exeDir = osh::getExecutableDir();

#if BX_PLATFORM_WINDOWS
    fs::path serverExe = exeDir / "limes-server.exe";
#else
    fs::path serverExe = exeDir / "limes-server";
#endif

    if (serverProcess)
    {
        stopServer();
    }
    serverProcess = new bp::child(serverExe.string());
}

void MainWindow::stopServer()
{
    if (serverProcess)
    {
        serverProcess->terminate();
        serverProcess->wait();
        delete serverProcess;
        serverProcess = nullptr;
    }
}

void MainWindow::onConnectToServer(Rml::DataModelHandle handle,
                                   Rml::Event& event,
                                   const Rml::VariantList& args)
{
    client.connectToServer(menuData.connectData.ipAddress,
                           menuData.connectData.udpPortServ,
                           menuData.connectData.tcpPortServ,
                           menuData.connectData.udpPortCli,
                           menuData.connectData.token);
    model.checkVersion(net::ModelClientInfo{
        .token = menuData.connectData.token,
        .ipAddress = menuData.connectData.ipAddress,
        .udpPortServ = menuData.connectData.udpPortServ,
        .tcpPortServ = menuData.connectData.tcpPortServ,
        .udpPortCli = menuData.connectData.udpPortCli,
    });
    userInterface.closeMenu();
}

void MainWindow::onCmd(const std::string& cmd)
{
    model.sendCmdToServer(cmd);
}

void MainWindow::onClientShutdown()
{
    LG_W("Client disconnected from server");
    model.disconnectFromServer();
}

void MainWindow::onQuit(Rml::DataModelHandle handle,
                        Rml::Event& event,
                        const Rml::VariantList& args)
{
    userInterface.closeMenu();
    client.shutdown();
    stopServer();
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void MainWindow::onExitToMenu(Rml::DataModelHandle handle,
                              Rml::Event& event,
                              const Rml::VariantList& args)
{
    client.shutdown();
}

void MainWindow::onAfterLoadWorld()
{
    renderEngine.setWorldShape(&model.getWorldShape());
}

}  // namespace ui