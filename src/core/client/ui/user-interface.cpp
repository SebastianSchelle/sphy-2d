#include "user-interface.hpp"
#include "RmlUi/Core/DataModelHandle.h"
#include "RmlUi/Core/ElementDocument.h"
#include "RmlUi/Core/EventListener.h"
#include "RmlUi/Core/ID.h"
#include "RmlUi/Core/Input.h"
#include "config-manager.hpp"
#include "dm-window.hpp"
#include "document-stack.hpp"
#include "event-listener.hpp"
#include "ptr-handle.hpp"
#include "safe-manager.hpp"
#include <GLFW/glfw3.h>
#include <RmlUi/Debugger.h>
#include <iomanip>
#include <limits>
#include <render-engine.hpp>
#include <sstream>
#include <work-distributor.hpp>

namespace ui
{

class ChatInputChangeListener final : public Rml::EventListener
{
  public:
    explicit ChatInputChangeListener(UserInterface* ui) : ui(ui) {}

    void ProcessEvent(Rml::Event& event) override
    {
        if (event != Rml::EventId::Change)
        {
            return;
        }
        if (!event.GetParameter<bool>("linebreak", false))
        {
            return;
        }
        if (ui)
        {
            ui->submitChatInput();
        }
    }

  private:
    UserInterface* ui;
};

void ChatData::addMessage(const ChatMessage& message)
{
    if (messages.size() > 50)
    {
        messages.erase(messages.begin());
    }
    messages.push_back(message);
    const auto tod = message.timestamp.time_of_day();
    std::ostringstream timeStream;
    timeStream << std::setfill('0') << std::setw(2) << tod.hours() << ":"
               << std::setw(2) << tod.minutes() << ":" << std::setw(2)
               << tod.seconds();
    messages.back().timestampText = timeStream.str();
}

UserInterface::UserInterface(cfg::ConfigManager& config,
                             ecs::PtrHandle* ptrHandle)
    : config(config), tabPanelStrategic(this, "tab-panel-strategic"),
      tabPanelTactical(this, "tab-panel-tactical"),
      menuStack([this](const string& id) { showDocument(id); },
                [this](const string& id) { hideDocument(id); },
                [this](const string& id) { return docVisible(id); }),
      ptrHandle(ptrHandle)
{
    chatData.currMsgTarget = "all";
}

UserInterface::~UserInterface() = default;

void UserInterface::setChatCmdHistoryMax(unsigned maxHistoryEntries)
{
    maxCmdHistoryEntries = maxHistoryEntries;
}

bool UserInterface::init(glm::ivec2 windowSize)
{
    // Create context (this will use the render interface)
    rmlContext = Rml::CreateContext("default",
                                    Rml::Vector2i(windowSize.x, windowSize.y));
    if (!rmlContext)
    {
        LG_E("Failed to create RmlUI context");
        return false;
    }

    // Init Rml debugger
    if (CFG_BOOL(config, 0.0f, "debug", "rml-debug"))
    {
        Rml::Debugger::Initialise(rmlContext);
        Rml::Debugger::SetContext(rmlContext);
        Rml::Debugger::SetVisible(true);
    }
    // Load default font
    if (!Rml::LoadFontFace("modules/engine/assets/ui/Roboto-Regular.ttf"))
    {
        LG_E("Failed to load default font");
        return false;
    }
    if (!Rml::LoadFontFace("modules/engine/assets/ui/Roboto-Bold.ttf"))
    {
        LG_E("Failed to load default font");
        return false;
    }
    if (!Rml::LoadFontFace("modules/engine/assets/ui/Roboto-Light.ttf"))
    {
        LG_E("Failed to load default font");
        return false;
    }
    if (!Rml::LoadFontFace("modules/engine/assets/ui/Roboto-Italic.ttf"))
    {
        LG_E("Failed to load default font");
        return false;
    }

    // Load mod loading ui. This UI is always needed from the start.
    UiDocHandle modLoadingHandle =
        loadDocument("mod-loading", "modules/engine/assets/ui/mod-loading.rml");
    if (!modLoadingHandle.isValid())
    {
        LG_E("Failed to load mod loading ui");
        return false;
    }

    float uiScale = CFG_FLOAT(config, 1.0f, "ui", "scale");
    rmlContext->SetDensityIndependentPixelRatio(uiScale);

    setupDataModels();
    setupChatDataModel();
    tabPanelStrategic.init();
    tabPanelTactical.init();
    // Test tab panel
    tabPanelStrategic.addTab("Object Info", "mp-obj-info");
    tabPanelStrategic.addTab("Faction Assets", "faction-assets");
    tabPanelStrategic.addTab("Relations", "relations");
    tabPanelTactical.addTab("Object Info", "mp-obj-info");

    // Test add some input events
    userInput.addEvent(InputEvent::Environment::General,
                       "Test Key",
                       "Test Key",
                       InputEvent::Key{
                           .key = GLFW_KEY_G,
                           .modifiers = 0,
                           .action = GLFW_PRESS,
                           .callback =
                               [this](const InputEvent::EventData& eventData)
                           {
                               LG_D("Test Key pressed");
                               return true;
                           },
                       });

    userInput.addEvent(InputEvent::Environment::General,
                       "Test Key Release",
                       "Test Key Release",
                       InputEvent::Key{
                           .key = GLFW_KEY_G,
                           .modifiers = 0,
                           .action = GLFW_RELEASE,
                           .callback =
                               [this](const InputEvent::EventData& eventData)
                           {
                               LG_D("Test Key released");
                               return true;
                           },
                       });

    return true;
}

void UserInterface::update()
{
    static tim::Timepoint lastUpdateTime = tim::getCurrentTimeU();
    static int i = 0;
    rmlContext->Update();
    if (focusChatInputOnNextUpdate && chatOpen)
    {
        focusChatInput();
        focusChatInputOnNextUpdate = false;
    }
    if (scrollChatOnNextUpdate)
    {
        scrollChatToBottom();
        scrollChatOnNextUpdate = false;
    }
}

void UserInterface::addChatMessage(const ChatMessage& message)
{
    chatData.addMessage(message);
    if (enableScrollDown)
    {
        rmlModelChat.DirtyVariable("messages");
        scrollChatOnNextUpdate = true;
    }
}

void UserInterface::addSystemMessage(const string& message)
{
    addChatMessage({"system", message, "me", tim::getCurrentTimeU()});
}

bool UserInterface::processMouseMove(glm::ivec2 mousePos, int keyMod)
{
    mouseOverUi = !rmlContext->ProcessMouseMove(mousePos.x, mousePos.y, keyMod);
    return mouseOverUi;
}

bool UserInterface::processMouseButtonDown(int button, int keyMod)
{
    if (button >= 3)
    {
        LG_E("Button index out of range");
        return false;
    }
    // Rml: true = mouse not interacting with any element (e.g. click missed
    // UI).
    const bool mouse_not_on_ui =
        rmlContext->ProcessMouseButtonDown(button, keyMod);
    mouseDownInteract[button] = !mouse_not_on_ui;

    if (button == 0 && mouse_not_on_ui && rmlContext)
    {
        if (Rml::Element* focused = rmlContext->GetFocusElement())
        {
            focused->Blur();
        }
    }

    return mouseDownInteract[button];
}

bool UserInterface::processMouseButtonUp(int button, int keyMod)
{
    if (button >= 3)
    {
        LG_E("Button index out of range");
        return false;
    }
    mouseUpInteract[button] = !rmlContext->ProcessMouseButtonUp(button, keyMod);
    return mouseUpInteract[button];
}

bool UserInterface::processKeyDown(Rml::Input::KeyIdentifier key)
{
    if (chatOpen && chatData.currMsgTarget == "cmd" && isChatInputFocused())
    {
        if (key == Rml::Input::KI_UP || key == Rml::Input::KI_DOWN)
        {
            if (handleCmdHistoryKey(key))
            {
                return false;
            }
        }
    }
    return rmlContext->ProcessKeyDown(Rml::Input::KeyIdentifier(key), 0);
}

bool UserInterface::processKeyUp(Rml::Input::KeyIdentifier key)
{
    return rmlContext->ProcessKeyUp(Rml::Input::KeyIdentifier(key), 0);
}

bool UserInterface::isMouseInteracting()
{
    return rmlContext->IsMouseInteracting();
}

bool UserInterface::processMouseWheel(int delta, int keyMod)
{
    float wheel = -delta * 1.0f;
    mouseWheelInteract = !rmlContext->ProcessMouseWheel(wheel, keyMod);
    return mouseWheelInteract;
}

void UserInterface::setDimensions(glm::ivec2 windowSize)
{
    rmlContext->SetDimensions(Rml::Vector2i(windowSize.x, windowSize.y));
}

void UserInterface::processTextInput(Rml::Character codepoint)
{
    rmlContext->ProcessTextInput(codepoint);
}

void UserInterface::render()
{
    rmlContext->Render();
}

UiDocHandle UserInterface::loadDocument(const std::string& name,
                                        const std::string& documentPath)
{
    Rml::ElementDocument* document = rmlContext->LoadDocument(documentPath);
    if (!document)
    {
        return UiDocHandle::Invalid();
    }
    UiDocHandle handle = rmlDocLib.addItem(name, document);

    if (name == "chat")
    {
        chatInputChangeListener =
            std::make_unique<ChatInputChangeListener>(this);
        if (Rml::Element* input = document->GetElementById("chat-input"))
        {
            input->AddEventListener(
                Rml::EventId::Change, chatInputChangeListener.get(), false);
        }
        else
        {
            LG_W("chat-input not found; Enter-to-send disabled");
            chatInputChangeListener.reset();
        }
    }

    return handle;
}

void UserInterface::unloadDocument(UiDocHandle handle)
{
    auto doc = rmlDocLib.getItem(handle);
    if (!doc)
    {
        return;
    }

    const UiDocHandle chatHandle = rmlDocLib.getHandle("chat");
    if (chatHandle.isValid() && handle.value() == chatHandle.value()
        && chatInputChangeListener)
    {
        if (Rml::Element* input = (*doc)->GetElementById("chat-input"))
        {
            input->RemoveEventListener(
                Rml::EventId::Change, chatInputChangeListener.get(), false);
        }
        chatInputChangeListener.reset();
    }

    rmlContext->UnloadDocument(*doc);
}

bool UserInterface::loadFont(const std::string& fontPath)
{
    return Rml::LoadFontFace(fontPath);
}

void UserInterface::showDocument(UiDocHandle handle)
{
    auto doc = rmlDocLib.getItem(handle);
    if (doc && !(*doc)->IsVisible())
    {
        LG_D("Showing document: {}", (*doc)->GetTitle());
        (*doc)->Show(Rml::ModalFlag::None, Rml::FocusFlag::Auto);
    }
}

void UserInterface::showDocument(const string& documentId)
{
    auto doc = rmlDocLib.getHandle(documentId);
    if (doc.isValid())
    {
        showDocument(doc);
    }
    else
    {
        LG_W("Document not found: {}", documentId);
    }
}

void UserInterface::hideDocument(UiDocHandle handle)
{
    auto doc = rmlDocLib.getItem(handle);
    if (doc && (*doc)->IsVisible())
    {
        LG_D("Hiding document: {}", (*doc)->GetTitle());
        (*doc)->Hide();
    }
}

void UserInterface::hideDocument(const string& documentId)
{
    auto doc = rmlDocLib.getHandle(documentId);
    if (doc.isValid())
    {
        hideDocument(doc);
    }
    else
    {
        LG_W("Document not found: {}", documentId);
    }
}


bool UserInterface::docVisible(UiDocHandle handle)
{
    auto doc = rmlDocLib.getItem(handle);
    return doc && (*doc)->IsVisible();
}

bool UserInterface::docVisible(const string& documentId)
{
    auto doc = rmlDocLib.getHandle(documentId);
    if (doc.isValid())
    {
        return docVisible(doc);
    }
    else
    {
        LG_W("Document not found: {}", documentId);
        return false;
    }
}


void UserInterface::hideAllDocuments()
{
    for (auto& doc : rmlDocLib.getItems())
    {
        (*doc)->Hide();
    }
    menuStack.clear();
    chatOpen = false;
    debugOpen = false;
    tabListStrategicOpen = false;
    tabListMap = false;
}

UiDocHandle UserInterface::getDocumentHandle(const std::string& name)
{
    return rmlDocLib.getHandle(name);
}

Rml::DataModelConstructor UserInterface::getDataModel(const std::string& name)
{
    auto modelConstructor = rmlContext->GetDataModel(name);
    if (modelConstructor)
    {
        return modelConstructor;
    }
    else
    {
        modelConstructor = rmlContext->CreateDataModel(name);
        if (modelConstructor)
        {
            return modelConstructor;
        }
        LG_E("Failed to create data model: {}", name);
        return Rml::DataModelConstructor();
    }
}

con::ItemLib<Rml::ElementDocument*>::Handle
UserInterface::getHandle(const string& name)
{
    auto handle = rmlDocLib.getHandle(name);
    if (!handle.isValid())
    {
        LG_E("Could not find ui document {}", name);
    }
    return handle;
}

void UserInterface::menuShow()
{
    if (menuStack.size())
    {
        menuStack.show();
    }
    else
    {
        menuStack.pushDocument("menu-root");
    }
}

void UserInterface::menuHide()
{
    menuStack.hide();
}

void UserInterface::menuPush(const string& id, const string& title)
{
    menuStack.pushDocument(id, title);
    dmMenu.title = menuStack.extra();
    dmhMenu.DirtyAllVariables();
}

void UserInterface::tipsShow()
{
    showDocument("tips");
}

void UserInterface::tipsHide()
{
    hideDocument("tips");
}

void UserInterface::hideTabListMap()
{
    hideDocument(rmlDocLib.getHandle("tab-list-map"));
    tabListMap = false;
}

void UserInterface::showTabListMap()
{
    if (!tabListMap)
    {
        showDocument(rmlDocLib.getHandle("tab-list-map"));
        tabListMap = true;
    }
}

void UserInterface::setupViewModeUi(gfx::GameViewMode viewMode)
{
    hideAllDocuments();
    switch (viewMode)
    {
        case gfx::GameViewMode::Map:
        {
            const string& currentTab =
                tabPanelStrategic.getCurrentTabDocumentId();
            if (tabPanelTactical.hasTab(currentTab))
            {
                showDocument(currentTab);
            }
            showTabListMap();
        }
        break;
        case gfx::GameViewMode::ThirdPerson:
        {
        }
        break;
        case gfx::GameViewMode::Menu:
        {
            menuShow();
        }
        break;
        case gfx::GameViewMode::AtlasDebug:
        {
            showDocument(getDocumentHandle("atlas-debug-menu"));
        }
        break;
        case gfx::GameViewMode::ModdingTools:
        {
            showDocument(getDocumentHandle("modding-tools-obj"));
            showDocument(getDocumentHandle("modding-tools-menu"));
        }
        break;
        case gfx::GameViewMode::Connecting:
        {
            showDocument(getDocumentHandle("connecting"));
        }
        break;
        default:
            break;
    }
}

void UserInterface::processEsc(bool allowClose)
{
    if (menuStack.isOpen())
    {
        menuStack.popDocument(allowClose);
    }
    else
    {
        menuStack.show();
    }
}

void UserInterface::onMenuBack(Rml::DataModelHandle handle,
                               Rml::Event& event,
                               const Rml::VariantList& args)
{
    menuStack.popDocument();
    if (menuStack.size())
    {
        dmMenu.title = menuStack.extra();
        dmhMenu.DirtyAllVariables();
    }
}

void UserInterface::onPrint(Rml::DataModelHandle handle,
                            Rml::Event& event,
                            const Rml::VariantList& args)
{
    if (args.size() > 1)
    {
        try
        {
            std::string target = args[0].Get<std::string>();
            std::string message = args[1].Get<std::string>();
            if (target == "debug")
            {
                LG_D("UI: {}", message);
            }
            else if (target == "info")
            {
                LG_I("UI: {}", message);
            }
            else if (target == "warning")
            {
                LG_W("UI: {}", message);
            }
            else if (target == "error")
            {
                LG_E("UI: {}", message);
            }
        }
        catch (const std::exception& e)
        {
            LG_E("UI: {}", e.what());
        }
        catch (...)
        {
            LG_E("UI: Unknown error");
        }
    }
}

void UserInterface::toggleChat()
{
    if (chatOpen)
    {
        // If chat input is focused, explicitly blur it before hiding the panel.
        // Otherwise hidden input can keep keyboard focus.
        if (rmlContext)
        {
            Rml::Element* el = rmlContext->GetFocusElement();
            while (el)
            {
                if (el->GetId() == "chat-input")
                {
                    el->Blur();
                    break;
                }
                el = el->GetParentNode();
            }
        }
        hideDocument(rmlDocLib.getHandle("chat"));
        chatOpen = false;
        focusChatInputOnNextUpdate = false;
    }
    else
    {
        showDocument(rmlDocLib.getHandle("chat"));
        chatOpen = true;
        enableScrollDown = true;
        scrollChatOnNextUpdate = true;
        focusChatInputOnNextUpdate = true;
    }
}

void UserInterface::toggleDebug()
{
    if (debugOpen)
    {
        hideDocument(rmlDocLib.getHandle("debug"));
        debugOpen = false;
    }
    else
    {
        showDocument(rmlDocLib.getHandle("debug"));
        debugOpen = true;
    }
}

void UserInterface::scrollChatToBottom()
{
    if (!rmlContext)
    {
        return;
    }

    auto chatDoc = rmlDocLib.getHandle("chat");
    if (!chatDoc.isValid())
    {
        return;
    }
    auto doc = rmlDocLib.getItem(chatDoc);
    if (doc)
    {
        auto chatElement = (*doc)->GetElementById("chat-scroll");
        if (chatElement)
        {
            chatElement->SetScrollTop(std::numeric_limits<float>::max());
        }
    }
}

void UserInterface::setupDataModels()
{
    auto constMenu = getDataModel("menu");
    DmWindow<DmNone>::RegisterType(constMenu);
    dmMenu = DmWindow<DmNone>{
        .title = "[menu.title]",
        .movable = false,
        .closable = false,
    };
    dmhMenu = constMenu.GetModelHandle();
    dmMenu.addButton(
        constMenu,
        {.id = "btnExit", .label = "[btn.exit]", .tooltip = "btn.exit.tooltip"},
        [this]() { ptrHandle->client->shutdown(); });
    dmMenu.addButton(constMenu,
                     {.id = "btnOptions",
                      .label = "[btn.options]",
                      .tooltip = "btn.options.tooltip",
                      .disabled = true},
                     [this]() { menuPush("menu-options", "[btn.options]"); });
    dmMenu.addButton(constMenu,
                     {.id = "btnNewGame",
                      .label = "[btn.newgame]",
                      .tooltip = "btn.newgame.tooltip"},
                     [this]() { menuPush("menu-new-game", "[btn.newgame]"); });
    dmMenu.addButton(constMenu,
                     {.id = "btnContinueGame",
                      .label = "[btn.continuegame]",
                      .tooltip = "btn.continuegame.tooltip"},
                     [this]()
                     {
                         vector<sphyc::SafeInfo> safes;
                         ptrHandle->safeManager->listSaveInfos(safes);
                         for (auto safe : safes)
                         {
                             LG_D("safe: {}", safe.name);
                         }
                     });
    dmMenu.addButton(constMenu,
                     {.id = "btnLoadGame",
                      .label = "[btn.loadgame]",
                      .tooltip = "btn.loadgame.tooltip"},
                     [this]()
                     { menuPush("menu-load-game", "[btn.loadgame]"); });
    // todo: script hook (modding) for registering menu elements in the data
    // model
    dmMenu.setup(constMenu, dmhMenu, {.onClose = [this]() { menuHide(); }});


    auto constTips = getDataModel("tips");
    DmWindow<DmTips>::RegisterType(constTips);
    dmTips = DmWindow<DmTips>{.title = "[tips.title]",
                              .movable = false,
                              .closable = false,
                              .data = {.tip = "[lorem400]"}};
    dmhTips = constTips.GetModelHandle();
    dmTips.setup(constTips, dmhTips, {});

    addPageEvents("load-game", {.onShow = []() { LG_D("Showed load game"); }});
}

void UserInterface::addPageEvents(const string& id,
                                  const PageEventListener::EventClbs& clbs)
{
    auto handle = rmlDocLib.getHandle(id);
    if (!handle.isValid())
        return;
    Rml::ElementDocument** doc = rmlDocLib.getItem(handle);
    if (!doc)
        return;
    PageEventListener listener(clbs);
    (*doc)->AddEventListener(Rml::EventId::Show, &listener, true);
}

void UserInterface::setupChatDataModel()
{
    auto chatConstructor = getDataModel("chat");

    if (auto md_handle = chatConstructor.RegisterStruct<ChatMessage>())
    {
        md_handle.RegisterMember("sender", &ChatMessage::sender);
        md_handle.RegisterMember("message", &ChatMessage::message);
        md_handle.RegisterMember("target", &ChatMessage::target);
        md_handle.RegisterMember("timestampText", &ChatMessage::timestampText);
    }

    chatConstructor.RegisterArray<std::vector<ChatMessage>>();
    chatConstructor.Bind("messages", &chatData.messages);
    chatConstructor.Bind("chat_input_text", &chatInputText);
    chatConstructor.Bind("curr_msg_target", &chatData.currMsgTarget);
    chatConstructor.BindEventCallback(
        "chat_send_msg", &UserInterface::onChatSendMsg, this);
    chatConstructor.BindEventCallback(
        "chat_scroll", &UserInterface::onChatScroll, this);
    chatConstructor.BindEventCallback(
        "chat_scroll_down", &UserInterface::onChatScrollDown, this);
    chatConstructor.BindEventCallback(
        "chat_sender_click", &UserInterface::onChatSenderClick, this);
    rmlModelChat = chatConstructor.GetModelHandle();
}

void UserInterface::onChatSendMsg(Rml::DataModelHandle handle,
                                  Rml::Event& event,
                                  const Rml::VariantList& args)
{
    (void)handle;
    (void)event;
    (void)args;
    LG_D("Chat send message");
    submitChatInput();
}

void UserInterface::onChatScroll(Rml::DataModelHandle handle,
                                 Rml::Event& event,
                                 const Rml::VariantList& args)
{
    (void)handle;
    (void)args;
    Rml::Element* const target = event.GetTargetElement();
    if (!target)
    {
        return;
    }
    enableScrollDown = isChatScrollNearBottom(target);
}

bool UserInterface::isChatScrollNearBottom(Rml::Element* scrollElement) const
{
    static constexpr float kThresholdPx = 48.f;
    if (!scrollElement)
    {
        return true;
    }
    const float scrollTop = scrollElement->GetScrollTop();
    const float scrollHeight = scrollElement->GetScrollHeight();
    const float clientHeight = scrollElement->GetClientHeight();
    const float maxScroll = scrollHeight - clientHeight;
    if (maxScroll <= 0.f)
    {
        return true;
    }
    return (maxScroll - scrollTop) <= kThresholdPx;
}

void UserInterface::onChatScrollDown(Rml::DataModelHandle handle,
                                     Rml::Event& event,
                                     const Rml::VariantList& args)
{
    (void)handle;
    (void)event;
    (void)args;
    enableScrollDown = true;
    scrollChatOnNextUpdate = true;
}

void UserInterface::onChatSenderClick(Rml::DataModelHandle handle,
                                      Rml::Event& event,
                                      const Rml::VariantList& args)
{
    (void)event;
    if (args.empty())
    {
        return;
    }
    const std::string sender = args[0].Get<std::string>();
    if (sender.empty())
    {
        return;
    }
    if (chatData.currMsgTarget != sender)
    {
        chatData.currMsgTarget = sender;
        handle.DirtyVariable("curr_msg_target");
        resetCmdHistoryBrowse();
    }
}

void UserInterface::submitChatInput()
{
    if (chatInputText.empty())
    {
        return;
    }
    InputMsgParseData parseData;
    if (!parseSendMsg(chatInputText, parseData))
    {
        return;
    }
    if (parseData.target != chatData.currMsgTarget)
    {
        chatData.currMsgTarget = parseData.target;
        rmlModelChat.DirtyVariable("curr_msg_target");
    }
    if (parseData.target == "cmd")
    {
        pushCmdHistory(parseData.message);
    }
    resetCmdHistoryBrowse();
    chatData.addMessage(
        {"me", parseData.message, parseData.target, tim::getCurrentTimeU()});
    chatInputText.clear();
    rmlModelChat.DirtyVariable("messages");
    rmlModelChat.DirtyVariable("chat_input_text");
    scrollChatOnNextUpdate = true;
}

void UserInterface::focusChatInput()
{
    if (!rmlContext || !chatOpen)
    {
        return;
    }
    const UiDocHandle chatDoc = rmlDocLib.getHandle("chat");
    if (!chatDoc.isValid())
    {
        return;
    }
    auto docPtr = rmlDocLib.getItem(chatDoc);
    if (!docPtr || !*docPtr)
    {
        return;
    }
    if (Rml::Element* input = (*docPtr)->GetElementById("chat-input"))
    {
        input->Focus();
    }
}

bool UserInterface::parseSendMsg(const string& message,
                                 InputMsgParseData& parseData)
{
    string remaining;
    if (message.empty())
    {
        return false;
    }
    if (message[0] == '/')
    {
        size_t spacePos = message.find(' ');
        string target = message.substr(1, spacePos - 1);
        if (target.empty())
        {
            LG_W("Invalid empty chat target");
            return false;
        }
        // Check if new target valid?
        parseData.target = target;
        remaining = message.substr(spacePos + 1);
        LG_D("Set new chat target: {}", target);
    }
    else
    {
        parseData.target = chatData.currMsgTarget;
        remaining = message;
    }
    if (parseData.target == "cmd")
    {
        LG_D("Command: {}", message);
        parseData.message = remaining;
        // todo: fix commands
        // cmdCallback(parseData.message);
        return true;
    }
    else
    {
        parseData.message = remaining;
    }
    return true;
}

bool UserInterface::isChatInputFocused() const
{
    if (!rmlContext || !chatOpen)
    {
        return false;
    }
    Rml::Element* el = rmlContext->GetFocusElement();
    while (el)
    {
        if (el->GetId() == "chat-input")
        {
            return true;
        }
        el = el->GetParentNode();
    }
    return false;
}

void UserInterface::resetCmdHistoryBrowse()
{
    cmdHistoryBrowseIndex = -1;
    cmdHistoryDraft.clear();
}

void UserInterface::pushCmdHistory(const std::string& cmd)
{
    if (maxCmdHistoryEntries == 0 || cmd.empty())
    {
        return;
    }
    if (!cmdHistory.empty() && cmdHistory.back() == cmd)
    {
        return;
    }
    cmdHistory.push_back(cmd);
    while (cmdHistory.size() > maxCmdHistoryEntries)
    {
        cmdHistory.erase(cmdHistory.begin());
    }
}

bool UserInterface::handleCmdHistoryKey(Rml::Input::KeyIdentifier key)
{
    if (cmdHistory.empty() || maxCmdHistoryEntries == 0)
    {
        return false;
    }
    if (key == Rml::Input::KI_UP)
    {
        if (cmdHistoryBrowseIndex < 0)
        {
            cmdHistoryDraft = chatInputText;
            cmdHistoryBrowseIndex = static_cast<int>(cmdHistory.size()) - 1;
        }
        else if (cmdHistoryBrowseIndex > 0)
        {
            cmdHistoryBrowseIndex--;
        }
        else
        {
            return true;
        }
        chatInputText = cmdHistory[static_cast<size_t>(cmdHistoryBrowseIndex)];
        rmlModelChat.DirtyVariable("chat_input_text");
        return true;
    }
    if (key == Rml::Input::KI_DOWN)
    {
        if (cmdHistoryBrowseIndex < 0)
        {
            return false;
        }
        if (cmdHistoryBrowseIndex < static_cast<int>(cmdHistory.size()) - 1)
        {
            cmdHistoryBrowseIndex++;
            chatInputText =
                cmdHistory[static_cast<size_t>(cmdHistoryBrowseIndex)];
        }
        else
        {
            cmdHistoryBrowseIndex = -1;
            chatInputText = cmdHistoryDraft;
        }
        rmlModelChat.DirtyVariable("chat_input_text");
        return true;
    }
    return false;
}

}  // namespace ui

template class con::ItemLib<Rml::ElementDocument*>;