#ifndef USER_INTERFACE_HPP
#define USER_INTERFACE_HPP

#include "RmlUi/Core/Core.h"
#include "RmlUi/Core/DataModelHandle.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <functional>
#include <item-lib.hpp>
#include <memory>
#include <ptr-handle.hpp>

using UiDocHandle = con::ItemLib<Rml::ElementDocument*>::Handle;

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
    // todo: instead of message, add vector<ChatChunk> where ChatChunk can be plain text or a hyperlink/reference or whatever
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
    UserInterface(CmdCallback cmdCallback);
    ~UserInterface();
    bool init(glm::ivec2 windowSize);
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
    void hideDocument(UiDocHandle handle);
    UiDocHandle getDocumentHandle(const std::string& name);
    void showMenu();
    void closeMenu();
    void processEsc(bool keepMenuOpen = false);
    void addSystemMessage(const string& message);
    void addChatMessage(const ChatMessage& message);
    void setChatCmdHistoryMax(unsigned maxHistoryEntries);
    UiDocHandle loadDocument(const std::string& name,
                             const std::string& documentPath);
    void unloadDocument(UiDocHandle handle);
    bool loadFont(const std::string& fontPath);
    // todo: move to mainwindow or something
    void
    bind(const std::string& model, const std::string& variable, void* value);
    void dirtyVar(const std::string& model, const std::string& variable);

    Rml::DataModelConstructor getDataModel(const std::string& name);


    void onMenuNavigate(Rml::DataModelHandle handle,
                        Rml::Event& event,
                        const Rml::VariantList& args);
    void onMenuBack(Rml::DataModelHandle handle,
                    Rml::Event& event,
                    const Rml::VariantList& args);
    void onPrint(Rml::DataModelHandle handle,
                 Rml::Event& event,
                 const Rml::VariantList& args);
    void toggleChat();
    void toggleDebug();
    bool isDebugOpen() const { return debugOpen; }
    bool isMenuOpen() const { return menuOpen; }

  private:
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

    con::ItemLib<Rml::ElementDocument*> rmlDocLib;
    Rml::Context* rmlContext;

    bool mouseOverUi;
    bool mouseDownInteract[3];
    bool mouseUpInteract[3];
    bool mouseWheelInteract;
    bool menuOpen = false;
    bool chatOpen = false;
    bool debugOpen = false;

    vector<string> menuStack;
    string currentMenuPage;

    ChatData chatData;
    std::string chatInputText;
    Rml::DataModelHandle rmlModelChat;
    bool scrollChatOnNextUpdate = false;
    bool focusChatInputOnNextUpdate = false;
    bool enableScrollDown = true;

    std::unique_ptr<ChatInputChangeListener> chatInputChangeListener;

    CmdCallback cmdCallback;
    unsigned maxCmdHistoryEntries = 50;
    std::vector<std::string> cmdHistory;
    int cmdHistoryBrowseIndex = -1;
    std::string cmdHistoryDraft;
};

}  // namespace ui


#endif