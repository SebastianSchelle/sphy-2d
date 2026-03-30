#include "user-interface.hpp"
#include "RmlUi/Core/DataModelHandle.h"
#include <iomanip>
#include <limits>
#include <sstream>

namespace ui
{


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

UserInterface::UserInterface(CmdCallback cmdCallback) : cmdCallback(cmdCallback) {
    chatData.currMsgTarget = "all";
}

UserInterface::~UserInterface() {}

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
    // Load default font
    if (!Rml::LoadFontFace("modules/engine/assets/ui/Orbitron-Regular.ttf"))
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

    setupChatDataModel();
    return true;
}

void UserInterface::update()
{
    static tim::Timepoint lastUpdateTime = tim::getCurrentTimeU();
    static int i = 0;
    DO_PERIODIC(lastUpdateTime,
                TIM_10S,
                [this]()
                {
                    chatData.addMessage({"System",
                                         "Test message " + std::to_string(i++),
                                         "Me",
                                         tim::getCurrentTimeU()});
                    rmlModelChat.DirtyVariable("messages");
                });
    // scrollChatOnNextUpdate = true;
    rmlContext->Update();
    if (scrollChatOnNextUpdate)
    {
        scrollChatToBottom();
        scrollChatOnNextUpdate = false;
    }
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
    mouseDownInteract[button] =
        !rmlContext->ProcessMouseButtonDown(button, keyMod);
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

    return handle;
}

void UserInterface::unloadDocument(UiDocHandle handle)
{
    auto doc = rmlDocLib.getItem(handle.getIdx());
    if (doc)
    {
        rmlContext->UnloadDocument(*doc);
    }
}

bool UserInterface::loadFont(const std::string& fontPath)
{
    return Rml::LoadFontFace(fontPath);
}

void UserInterface::showDocument(UiDocHandle handle)
{
    auto doc = rmlDocLib.getItem(handle.getIdx());
    if (doc)
    {
        LG_D("Showing document: {}", (*doc)->GetTitle());
        (*doc)->Show(Rml::ModalFlag::None, Rml::FocusFlag::Auto);
    }
}

void UserInterface::hideDocument(UiDocHandle handle)
{
    auto doc = rmlDocLib.getItem(handle.getIdx());
    if (doc)
    {
        LG_D("Hiding document: {}", (*doc)->GetTitle());
        (*doc)->Hide();
    }
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

void UserInterface::showMenu()
{
    currentMenuPage = "menu";
    menuStack.clear();
    showDocument(rmlDocLib.getHandle(currentMenuPage));
    menuOpen = true;
}

void UserInterface::closeMenu()
{
    hideDocument(rmlDocLib.getHandle(currentMenuPage));
    menuOpen = false;
}

void UserInterface::processEsc(bool keepMenuOpen)
{
    if (menuStack.empty())
    {
        if (menuOpen)
        {
            if (!keepMenuOpen)
            {
                closeMenu();
            }
        }
        else
        {
            showMenu();
        }
    }
    else
    {
        onMenuBackPriv();
    }
}


void UserInterface::onMenuNavigate(Rml::DataModelHandle handle,
                                   Rml::Event& event,
                                   const Rml::VariantList& args)
{
    if (args.size() > 0)
    {
        std::string target = args[0].Get<std::string>();
        UiDocHandle doc = rmlDocLib.getHandle(target);
        if (doc.isValid())
        {
            menuStack.push_back(currentMenuPage);
            hideDocument(rmlDocLib.getHandle(currentMenuPage));
            currentMenuPage = target;
            showDocument(doc);
        }
        else
        {
            LG_W("Document not found: {}", target);
        }
    }
    else
    {
        LG_W("No target provided");
    }
}

void UserInterface::onMenuBack(Rml::DataModelHandle handle,
                               Rml::Event& event,
                               const Rml::VariantList& args)
{
    onMenuBackPriv();
}

void UserInterface::onMenuBackPriv()
{
    if (!menuStack.empty())
    {
        UiDocHandle doc = rmlDocLib.getHandle(menuStack.back());
        if (doc.isValid())
        {
            hideDocument(rmlDocLib.getHandle(currentMenuPage));
            currentMenuPage = menuStack.back();
            menuStack.pop_back();
            showDocument(doc);
            return;
        }
    }
    closeMenu();
}

void UserInterface::onQuit(Rml::DataModelHandle handle,
                           Rml::Event& event,
                           const Rml::VariantList& args)
{
    closeMenu();
    exit(0);
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
        LG_D("Closing chat");
        hideDocument(rmlDocLib.getHandle("chat"));
        chatOpen = false;
    }
    else
    {
        LG_D("Opening chat");
        showDocument(rmlDocLib.getHandle("chat"));
        chatOpen = true;
        scrollChatOnNextUpdate = true;
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
    auto doc = rmlDocLib.getItem(chatDoc.getIdx());
    if (doc)
    {
        auto chatElement = (*doc)->GetElementById("chat-scroll");
        if (chatElement)
        {
            chatElement->SetScrollTop(std::numeric_limits<float>::max());
        }
    }
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
    rmlModelChat = chatConstructor.GetModelHandle();
}

void UserInterface::onChatSendMsg(Rml::DataModelHandle handle,
                                  Rml::Event& event,
                                  const Rml::VariantList& args)
{
    LG_D("Chat send message: {}", args.size());
    if (args.size() > 0)
    {
        std::string message = args[0].Get<std::string>();
        if (message.empty())
        {
            return;
        }
        InputMsgParseData parseData;
        if (!parseSendMsg(message, parseData))
        {
            return;
        }
        if(parseData.target != chatData.currMsgTarget)
        {
            chatData.currMsgTarget = parseData.target;
            rmlModelChat.DirtyVariable("curr_msg_target");
        }
        chatData.addMessage({"Me",
                             parseData.message,
                             parseData.target,
                             tim::getCurrentTimeU()});
        chatInputText.clear();
        rmlModelChat.DirtyVariable("messages");
        rmlModelChat.DirtyVariable("chat_input_text");
        scrollChatOnNextUpdate = true;
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
        cmdCallback(parseData.message);
        return true;
    }
    else
    {
        parseData.message = remaining;
    }
    return true;
}

}  // namespace ui

template class con::ItemLib<Rml::ElementDocument*>;