#ifndef USER_INTERFACE_HPP
#define USER_INTERFACE_HPP

#include "RmlUi/Core/Core.h"
#include "RmlUi/Core/DataModelHandle.h"
#include "config-manager.hpp"
#include "document-stack.hpp"
#include "event-listener.hpp"
#include "ptr-handle.hpp"
#include "ui-tab-panel.hpp"
#include "user-input.hpp"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <dm-if.hpp>
#include <functional>
#include <item-lib.hpp>
#include <memory>

using UiDocHandle = con::ItemLib<Rml::ElementDocument*>::Handle;

namespace gfx
{
enum class GameViewMode : uint8_t;
}

namespace ui
{

typedef std::function<void(const std::string&)> CmdCallback;

class Document
{
    con::ItemLib<Rml::ElementDocument*> rmlDocLib;
};

struct ChatMessage
{
    string sender;
    string message;
    // todo: instead of message, add vector<ChatChunk> where ChatChunk can be
    // plain text or a hyperlink/reference or whatever
    string target;
    tim::Timepoint timestamp;
    string timestampText;
};

struct InputMsgParseData
{
    string target;
    string message;
};

struct ChatData
{
    vector<ChatMessage> messages;
    string currMsgTarget;
    void addMessage(const ChatMessage& message);
};

class ChatInputChangeListener;

class UserInterface
{
  public:
    UserInterface(cfg::ConfigManager& config, ecs::PtrHandle* ptrHandle);
    ~UserInterface();
    bool init(glm::ivec2 windowSize);
    bool postInit();
    void update();
    void setDimensions(glm::ivec2 windowSize);
    bool processMouseMove(glm::ivec2 mousePos, int keyMod);
    bool processMouseButtonDown(int button, int keyMod);
    bool processMouseButtonUp(int button, int keyMod);
    bool isMouseInteracting();
    bool processMouseWheel(int delta, int keyMod);
    bool processKeyDown(Rml::Input::KeyIdentifier key);
    bool processKeyUp(Rml::Input::KeyIdentifier key);
    void processTextInput(Rml::Character codepoint);
    void render();
    void showDocument(UiDocHandle handle);
    void showDocument(const string& documentId);
    void hideDocument(UiDocHandle handle);
    void hideDocument(const string& documentId);
    bool docVisible(UiDocHandle handle);
    bool docVisible(const string& documentId);
    void hideAllDocuments();
    UiDocHandle getDocumentHandle(const std::string& name);
    void menuShow();
    void menuHide();
    void menuPush(const string& id, const string& title);
    void tipsShow();
    void tipsHide();
    void showConnecting();
    void hideConnecting();
    void hideTabListMap();
    void showTabListMap();
    void processEsc(bool allowClose = true);
    void addSystemMessage(const string& message);
    void addChatMessage(const ChatMessage& message);
    void setChatCmdHistoryMax(unsigned maxHistoryEntries);
    UiDocHandle loadDocument(const std::string& name,
                             const std::string& documentPath);
    void unloadDocument(UiDocHandle handle);
    bool loadFont(const std::string& fontPath);
    void setupViewModeUi(gfx::GameViewMode viewMode);

    Rml::DataModelConstructor getDataModel(const std::string& name);

    void onMenuBack(Rml::DataModelHandle handle,
                    Rml::Event& event,
                    const Rml::VariantList& args);
    void onPrint(Rml::DataModelHandle handle,
                 Rml::Event& event,
                 const Rml::VariantList& args);
    void toggleChat();
    void toggleDebug();
    bool isDebugOpen() const
    {
        return debugOpen;
    }
    bool isMenuOpen() const
    {
        return menuStack.isOpen();
    }
    UserInput& getUserInput()
    {
        return userInput;
    }
    InputEvent::Environment getUiEnvironment() const
    {
        return uiEnvironment;
    }
    void setUiEnvironment(InputEvent::Environment environment)
    {
        uiEnvironment = environment;
    }
    void addPageEvents(const string& id, PageEventListener& listener);

  private:
    con::ItemLib<Rml::ElementDocument*>::Handle getHandle(const string& name);
    void onMenuBackPriv();
    void setupChatDataModel();
    void scrollChatToBottom();
    void onChatSendMsg(Rml::DataModelHandle handle,
                       Rml::Event& event,
                       const Rml::VariantList& args);
    void onChatScroll(Rml::DataModelHandle handle,
                      Rml::Event& event,
                      const Rml::VariantList& args);
    void onChatScrollDown(Rml::DataModelHandle handle,
                          Rml::Event& event,
                          const Rml::VariantList& args);
    void onChatSenderClick(Rml::DataModelHandle handle,
                           Rml::Event& event,
                           const Rml::VariantList& args);
    bool isChatScrollNearBottom(Rml::Element* scrollElement) const;

    void submitChatInput();
    void focusChatInput();
    bool parseSendMsg(const string& message, InputMsgParseData& parseData);
    bool isChatInputFocused() const;
    bool handleCmdHistoryKey(Rml::Input::KeyIdentifier key);
    void pushCmdHistory(const std::string& cmd);
    void resetCmdHistoryBrowse();

    friend class ChatInputChangeListener;

    DmIf dmIf;

    con::ItemLib<Rml::ElementDocument*> rmlDocLib;
    Rml::Context* rmlContext;
    ecs::PtrHandle* ptrHandle;

    bool mouseOverUi;
    bool mouseDownInteract[3];
    bool mouseUpInteract[3];
    bool mouseWheelInteract;
    bool chatOpen = false;
    bool debugOpen = false;
    bool tabListStrategicOpen = false;
    bool tabListMap = false;

    DocumentStack<string> menuStack;

    cfg::ConfigManager& config;
    ChatData chatData;
    std::string chatInputText;
    Rml::DataModelHandle rmlModelChat;
    bool scrollChatOnNextUpdate = false;
    bool focusChatInputOnNextUpdate = false;
    bool enableScrollDown = true;

    std::unique_ptr<ChatInputChangeListener> chatInputChangeListener;

    unsigned maxCmdHistoryEntries = 50;
    std::vector<std::string> cmdHistory;
    int cmdHistoryBrowseIndex = -1;
    std::string cmdHistoryDraft;

    UiTabPanel tabPanelStrategic;
    UiTabPanel tabPanelTactical;

    UserInput userInput;

    InputEvent::Environment uiEnvironment = InputEvent::Environment::General;
};

}  // namespace ui


#endif