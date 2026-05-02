#include "GLFW/glfw3.h"
#include "RmlUi/Core/Core.h"
#include "bgfx/defines.h"
#include "std-inc.hpp"
#include "vertex-defines.hpp"
#include <bgfx/platform.h>
#include <bx/bx.h>
#include <comp-ident.hpp>
#include <main-window.hpp>
#include <memory>
#include <os-helper.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

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


void MouseState::processMouseButton(uint8_t i, float zoom, float dragThreshold)
{
    buttonReleased[i] = false;
    singleClick[i] = false;
    doubleClick[i] = false;
    longClick[i] = false;
    buttonPressed[i] = false;
    hold[i] = false;
    bool dragActiveLast = dragActive[i];
    dragActive[i] = false;
    dragFinished[i] = false;

    if (buttons[i] && !lastButtons[i])
    {
        timePressed[i] = tim::getCurrentTimeU();
        mouseCoordsPressed[i] = mouseCoords;
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
        mouseCoordsReleased[i] = mouseCoords;
        long clickDuration = tim::durationU(timePressed[i], now);
        if (dragActiveLast)
        {
            dragFinished[i] = true;
        }
        else if (clickDuration > 300000U)
        {
            longClick[i] = true;
        }
        else
        {
            singleClick[i] = true;
            lastSingleClick[i] = now;
        }
    }
    else if (buttons[i]
             && (mouseCoords.pos != mouseCoordsPressed[i].pos
                 || glm::length2(mouseCoords.sectorPos
                                 - mouseCoordsPressed[i].sectorPos)
                        >= dragThreshold / (zoom * zoom)))
    {
        dragActive[i] = true;
    }
}

MainWindow::MainWindow(sphy::CmdLinOptionsClient& options)
    : options(options),
      config(options.workingdir + "/modules/core/defs/client.yaml"),
      renderEngine(config), rmlUiRenderInterface(&renderEngine),
      client(config, model.sendQueue, model.receiveQueue), modManager(),
      modLoadingHandle(UiDocHandle::Invalid()),
      userInterface(std::bind(&MainWindow::onCmd, this, std::placeholders::_1)),
      model(&userInterface,
            config,
            &modManager,
            &renderEngine,
            std::bind(&MainWindow::onAfterLoadWorld, this))
{
    auto path(options.workingdir);
    std::filesystem::current_path(path);

    uint8_t logLevel = CFG_UINT(config, 1.0f, "loglevel");
    debug::createLogger("logs/logClient.txt", logLevel);
    dragThreshold =
        CFG_FLOAT(config, 300.0f, "input", "drag-threshold", "world");
    dragBoxColor = CFG_UINT(
        config, (float)0x2085e085, "theme", "input", "drag-box", "color");
    dragBoxThickness =
        CFG_FLOAT(config, 1.0f, "theme", "input", "drag-box", "thickness");
    const unsigned chatCmdHistoryEntries =
        CFG_UINT(config, 50.0f, "chat", "cmd-history-entries");
    userInterface.setChatCmdHistoryMax(chatCmdHistoryEntries);

    LG_I("drag threshold: 0x{:x}", dragBoxColor);

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
    moddingTools.setupDataModel(userInterface);

    return true;
}

bool MainWindow::initPost()
{
    return true;
}

bool MainWindow::createWindow()
{
    uint32_t wWidth = CFG_UINT(config, 1200.0f, "win", "width");
    uint32_t wHeight = CFG_UINT(config, 800.0f, "win", "height");

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
    tim::Timepoint lastSelectedEntitiesMoveCmd = tim::getCurrentTimeU();

    while (!glfwWindowShouldClose(window))
    {
        processUiTasks();

        tim::Timepoint now = tim::getCurrentTimeU();
        float deltaTime = (float)tim::durationU(lastLoopTime, now) / 1000000.0f;
        lastLoopTime = now;
        frameTimeFiltered = 0.9f * frameTimeFiltered + 0.1f * deltaTime;
        debugData.viewData.fpsSmoothed =
            frameTimeFiltered > 1e-6f ? 1.0f / frameTimeFiltered : 0.f;

        model.modelLoop(deltaTime);

        mouseState.mz = 0;
        glfwPollEvents();
        setupMouseState();
        handleWinResize();

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

        ProcessMouseStateNoui();

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

        renderEngine.startFrame();

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
                renderMenu();
                break;
            case ClientGameState::Authenticated:
                break;
            case ClientGameState::LoadWorld:
                break;
            case ClientGameState::GameLoop:
                renderGame();
                break;
            case ClientGameState::ModdingTools:
                break;
            default:
                break;
        }

        if (model.getGameState() == ClientGameState::GameLoop)
        {
        }

        userInterface.render();
        renderEngine.endFrame();
    }
}

void MainWindow::renderMenu()
{
    renderEngine.drawFullScreenTriangles(
        0, renderEngine.getShaderHandle("distantstars"));

    static float rot = 0.0f;
    renderEngine.drawTexRect(glm::vec2(0.0f, 0.0f),
                             glm::vec2(100.0f, 10.0f),
                             renderEngine.getTextureHandle("test"),
                             rot,
                             0);
    rot += 0.01f;
}

void MainWindow::renderGame()
{
    float zoom = renderEngine.getWorldZoom();
    Rect viewportRect;
    renderEngine.getViewportRect(viewportRect);
    renderEngine.drawFullScreenTriangles(
        0, renderEngine.getShaderHandle("distantstars"));
    switch (renderEngine.getViewMode())
    {
        case gfx::GameViewMode::StrategicMap:
            processMouseTactical(zoom);
            model.drawStrategicMap(renderEngine, viewportRect, zoom);
            renderEngine.panWorld(panX, panY);
            break;
        case gfx::GameViewMode::TacticalMap:
            processMouseTactical(zoom);
            model.drawTacticalMap(renderEngine, viewportRect, zoom);
            renderEngine.panWorld(panX, panY);
            break;
        case gfx::GameViewMode::ThirdPerson:
            model.drawThirdPerson(renderEngine, viewportRect, zoom);
            renderEngine.panWorld(panX, panY);
            break;
        default:
            break;
    }
}

void MainWindow::renderModdingTools()
{
}

void MainWindow::processMouseTactical(float zoom)
{
    if (mouseState.dragActive[0])
    {
        drawWorldRectangle(mouseState.mouseCoordsPressed[0],
                           mouseState.mouseCoords,
                           dragBoxColor,
                           dragBoxThickness / zoom,
                           0.0f);
    }
    if (mouseState.dragFinished[0])
    {
        model.selectEntitiesInsideRect(mouseState.mouseCoordsPressed[0],
                                       mouseState.mouseCoordsReleased[0]);
    }
    else if (mouseState.singleClick[0])
    {
        //model.selectEntityAtWorldPosFast(mouseState.mouseCoords, 10.0f/zoom * 10.0f/zoom);
        model.selectEntityAtWorldPos(mouseState.mouseCoords);
    }
    if (mouseState.buttons[1] && model.getSelectedEntities().size() > 0)
    {
        const bool shiftPressed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
                                  || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT)
                                         == GLFW_PRESS;
        model.selectedEntitiesMoveCmd(mouseState.mouseCoords, shiftPressed);
    }
}

void MainWindow::drawWorldRectangle(const def::SectorCoords& A,
                                    const def::SectorCoords& B,
                                    uint32_t colorABGR,
                                    float thickness,
                                    float rotationRad)
{
    vec2 offsA = model.getWorld().getWorldPosSectorOffset(
        A.pos.x,
        A.pos.y,
        renderEngine.getSectorOffsetX(),
        renderEngine.getSectorOffsetY());
    vec2 offsB = model.getWorld().getWorldPosSectorOffset(
        B.pos.x,
        B.pos.y,
        renderEngine.getSectorOffsetX(),
        renderEngine.getSectorOffsetY());
    vec2 a = offsA + A.sectorPos;
    vec2 b = offsB + B.sectorPos;
    vec2 pos = (a + b) / 2.0f;
    vec2 size = glm::abs(b - a);
    renderEngine.drawRectangle(pos, size, colorABGR, thickness, rotationRad);
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
            model.startModel();
        }
        else
        {
            LG_E("Failed to load mods");
            exit(1);
        }
    }
}

void MainWindow::setupMouseState()
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
}

void MainWindow::ProcessMouseStateNoui()
{
    renderEngine.screenToSectorCoords(mouseState.mousePos,
                                      mouseState.mouseCoords);

    float zoom = renderEngine.getWorldZoom();
    mouseState.processMouseButton(0, zoom, dragThreshold);
    mouseState.processMouseButton(1, zoom, dragThreshold);
    mouseState.processMouseButton(2, zoom, dragThreshold);

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
    debugData.inputData.ptrSectorX = mouseState.mouseCoords.pos.x;
    debugData.inputData.ptrSectorY = mouseState.mouseCoords.pos.y;
    debugData.inputData.ptrSectorPosX = mouseState.mouseCoords.sectorPos.x;
    debugData.inputData.ptrSectorPosY = mouseState.mouseCoords.sectorPos.y;
    debugData.gameData.gameState = magic_enum::enum_name(model.getGameState());
    debugData.gameData.viewMode = magic_enum::enum_name(renderEngine.getViewMode());
    debugData.connectionData.serverLatency =
        model.getTimeSyncData().serverLatency;
    debugData.connectionData.serverTimeOffset =
        model.getTimeSyncData().serverOffset;
    const bool modelAabbOverlayEnabled = model.isAabbTreeOverlayEnabled();
    if (debugData.overlayData.enableAabbTree != modelAabbOverlayEnabled)
    {
        model.setOverlayEnabled("aabb-tree",
                                debugData.overlayData.enableAabbTree);
    }
    else
    {
        debugData.overlayData.enableAabbTree = modelAabbOverlayEnabled;
    }
    debugData.viewData.sectorOffsetX = renderEngine.getSectorOffsetX();
    debugData.viewData.sectorOffsetY = renderEngine.getSectorOffsetY();

    auto& reg = model.getRegistry();
    auto& go = debugData.selGameObject;
    go.entityId = model.getSelectedEntity().index;
    go.generation = model.getSelectedEntity().generation;
    entt::entity entity =
        model.getEcs()->enttFromServerId(model.getSelectedEntity());
    go.hasTransform = false;
    go.hasPhysicsBody = false;
    go.hasPhyThrust = false;
    go.hasSectorId = false;
    go.sectorId = 0;
    if (entity != entt::null && reg.valid(entity))
    {
        go.posX = go.posY = go.rot = 0.f;
        go.mass = go.inertia = 0.f;
        go.velX = go.velY = go.rotVel = 0.f;
        go.thrustGlobalX = go.thrustGlobalY = go.thrustLocalX =
            go.thrustLocalY = 0.f;
        go.torque = go.maxTorque = go.maxRotVel = 0.f;
        go.thrustMainMax = go.thrustManeuverMax = go.maxSpd = 0.f;
        if (auto sectorIdComp = reg.try_get<ecs::SectorId>(entity))
        {
            go.hasSectorId = true;
            go.sectorId = sectorIdComp->id;
        }
        if (auto transform = reg.try_get<ecs::Transform>(entity))
        {
            go.hasTransform = true;
            go.posX = transform->pos.x;
            go.posY = transform->pos.y;
            go.rot = transform->rot;
        }
        if (auto physicsBody = reg.try_get<ecs::PhysicsBody>(entity))
        {
            go.hasPhysicsBody = true;
            go.mass = physicsBody->mass;
            go.inertia = physicsBody->inertia;
            go.velX = physicsBody->vel.x;
            go.velY = physicsBody->vel.y;
            go.rotVel = physicsBody->rotVel;
        }
        if (auto phyThrust = reg.try_get<ecs::PhyThrust>(entity))
        {
            go.hasPhyThrust = true;
            go.thrustGlobalX = phyThrust->thrustGlobal.x;
            go.thrustGlobalY = phyThrust->thrustGlobal.y;
            go.thrustLocalX = phyThrust->thrustLocal.x;
            go.thrustLocalY = phyThrust->thrustLocal.y;
            go.torque = phyThrust->torque;
            go.maxTorque = phyThrust->maxTorque;
            go.maxRotVel = phyThrust->maxRotVel;
            go.thrustMainMax = phyThrust->thrustMainMax;
            go.thrustManeuverMax = phyThrust->thrustManeuverMax;
            go.maxSpd = phyThrust->maxSpd;
        }
        if (auto moveCtrl = reg.try_get<ecs::MoveCtrl>(entity))
        {
            go.hasMoveCtrl = true;
            go.moveCtrlActive = moveCtrl->active;
            go.spPosX = moveCtrl->spPos.sectorPos.x;
            go.spPosY = moveCtrl->spPos.sectorPos.y;
            go.spPosSecX = moveCtrl->spPos.pos.x;
            go.spPosSecY = moveCtrl->spPos.pos.y;
            go.spRot = moveCtrl->spRot;
            go.moveCtrlFaceDirMode =
                magic_enum::enum_name(moveCtrl->faceDirMode);
            go.lookAtX = moveCtrl->lookAt.x;
            go.lookAtY = moveCtrl->lookAt.y;
        }
    }
    else
    {
        go.hasSectorId = false;
        go.sectorId = 0;
        go.posX = go.posY = go.rot = 0.f;
        go.mass = go.inertia = 0.f;
        go.velX = go.velY = go.rotVel = 0.f;
        go.thrustGlobalX = go.thrustGlobalY = go.thrustLocalX =
            go.thrustLocalY = 0.f;
        go.torque = go.maxTorque = go.maxRotVel = 0.f;
        go.thrustMainMax = go.thrustManeuverMax = go.maxSpd = 0.f;
        go.hasMoveCtrl = false;
        go.moveCtrlActive = false;
        go.moveCtrlFaceDirMode = "None";
        go.spPosSecX = 0;
        go.spPosSecY = 0;
        go.spPosX = 0.f;
        go.spPosY = 0.f;
        go.lookAtX = 0.f;
        go.lookAtY = 0.f;
    }
}

void MainWindow::setupDataModelDebug()
{
    auto debugConstructor = userInterface.getDataModel("debug");
    if (debugConstructor)
    {
        if (auto md_handle =
                debugConstructor.RegisterStruct<UiDebugGameObject>())
        {
            md_handle.RegisterMember("entityId", &UiDebugGameObject::entityId);
            md_handle.RegisterMember("generation",
                                     &UiDebugGameObject::generation);
            md_handle.RegisterMember("hasSectorId",
                                     &UiDebugGameObject::hasSectorId);
            md_handle.RegisterMember("sectorId", &UiDebugGameObject::sectorId);
            md_handle.RegisterMember("hasTransform",
                                     &UiDebugGameObject::hasTransform);
            md_handle.RegisterMember("posX", &UiDebugGameObject::posX);
            md_handle.RegisterMember("posY", &UiDebugGameObject::posY);
            md_handle.RegisterMember("rot", &UiDebugGameObject::rot);
            md_handle.RegisterMember("hasPhysicsBody",
                                     &UiDebugGameObject::hasPhysicsBody);
            md_handle.RegisterMember("mass", &UiDebugGameObject::mass);
            md_handle.RegisterMember("inertia", &UiDebugGameObject::inertia);
            md_handle.RegisterMember("velX", &UiDebugGameObject::velX);
            md_handle.RegisterMember("velY", &UiDebugGameObject::velY);
            md_handle.RegisterMember("rotVel", &UiDebugGameObject::rotVel);
            md_handle.RegisterMember("hasPhyThrust",
                                     &UiDebugGameObject::hasPhyThrust);
            md_handle.RegisterMember("thrustGlobalX",
                                     &UiDebugGameObject::thrustGlobalX);
            md_handle.RegisterMember("thrustGlobalY",
                                     &UiDebugGameObject::thrustGlobalY);
            md_handle.RegisterMember("thrustLocalX",
                                     &UiDebugGameObject::thrustLocalX);
            md_handle.RegisterMember("thrustLocalY",
                                     &UiDebugGameObject::thrustLocalY);
            md_handle.RegisterMember("torque", &UiDebugGameObject::torque);
            md_handle.RegisterMember("maxTorque",
                                     &UiDebugGameObject::maxTorque);
            md_handle.RegisterMember("maxRotVel",
                                     &UiDebugGameObject::maxRotVel);
            md_handle.RegisterMember("thrustMainMax",
                                     &UiDebugGameObject::thrustMainMax);
            md_handle.RegisterMember("thrustManeuverMax",
                                     &UiDebugGameObject::thrustManeuverMax);
            md_handle.RegisterMember("maxSpd", &UiDebugGameObject::maxSpd);
            md_handle.RegisterMember("hasMoveCtrl",
                                     &UiDebugGameObject::hasMoveCtrl);
            md_handle.RegisterMember("moveCtrlActive",
                                     &UiDebugGameObject::moveCtrlActive);
            md_handle.RegisterMember("spPosX", &UiDebugGameObject::spPosX);
            md_handle.RegisterMember("spPosY", &UiDebugGameObject::spPosY);
            md_handle.RegisterMember("spPosSecX",
                                     &UiDebugGameObject::spPosSecX);
            md_handle.RegisterMember("spPosSecY",
                                     &UiDebugGameObject::spPosSecY);
            md_handle.RegisterMember("spRot", &UiDebugGameObject::spRot);
            md_handle.RegisterMember("moveCtrlFaceDirMode",
                                     &UiDebugGameObject::moveCtrlFaceDirMode);
            md_handle.RegisterMember("lookAtX", &UiDebugGameObject::lookAtX);
            md_handle.RegisterMember("lookAtY", &UiDebugGameObject::lookAtY);
        }
        debugConstructor.Bind("selGameObject", &debugData.selGameObject);

        if (auto md_handle = debugConstructor.RegisterStruct<UiDbgInputData>())
        {
            md_handle.RegisterMember("ptrScreenX", &UiDbgInputData::ptrScreenX);
            md_handle.RegisterMember("ptrScreenY", &UiDbgInputData::ptrScreenY);
            md_handle.RegisterMember("ptrOverUi", &UiDbgInputData::ptrOverUi);
            md_handle.RegisterMember("ptrSectorX", &UiDbgInputData::ptrSectorX);
            md_handle.RegisterMember("ptrSectorY", &UiDbgInputData::ptrSectorY);
            md_handle.RegisterMember("ptrSectorPosX",
                                     &UiDbgInputData::ptrSectorPosX);
            md_handle.RegisterMember("ptrSectorPosY",
                                     &UiDbgInputData::ptrSectorPosY);
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
            md_handle.RegisterMember("sectorOffsetX",
                                     &UiDbgViewData::sectorOffsetX);
            md_handle.RegisterMember("sectorOffsetY",
                                     &UiDbgViewData::sectorOffsetY);
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

        if (auto md_handle =
                debugConstructor.RegisterStruct<UiDbgOverlayData>())
        {
            md_handle.RegisterMember("enableAabbTree",
                                     &UiDbgOverlayData::enableAabbTree);
        }
        debugConstructor.Bind("overlayData", &debugData.overlayData);

        if (auto md_handle = debugConstructor.RegisterStruct<UiDbgGameData>())
        {
            md_handle.RegisterMember("gameState", &UiDbgGameData::gameState);
            md_handle.RegisterMember("viewMode", &UiDbgGameData::viewMode);
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
            "onStartModdingTools", &MainWindow::onStartModdingTools, this);
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
    serverProcess = new boost::process::v1::child(serverExe.string());
}

void MainWindow::onStartModdingTools(Rml::DataModelHandle handle,
                                     Rml::Event& event,
                                     const Rml::VariantList& args)
{
    moddingTools.openToolsUi(userInterface);
    model.startModdingTools();
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