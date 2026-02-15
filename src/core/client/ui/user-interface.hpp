#ifndef USER_INTERFACE_HPP
#define USER_INTERFACE_HPP

#include "RmlUi/Core/Core.h"
#include "RmlUi/Core/DataModelHandle.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <item-lib.hpp>

using UiDocHandle = con::ItemLib<Rml::ElementDocument*>::Handle;

namespace ui
{

class Document
{
    con::ItemLib<Rml::ElementDocument*> rmlDocLib;
};

class UserInterface
{
  public:
    UserInterface();
    ~UserInterface();
    bool init(glm::ivec2 windowSize);
    void update();
    void setDimensions(glm::ivec2 windowSize);
    void processMouseMove(glm::ivec2 mousePos, int keyMod);
    void processMouseButtonDown(int button, int keyMod);
    void processMouseButtonUp(int button, int keyMod);
    bool isMouseInteracting();
    void processMouseWheel(int delta, int keyMod);
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
    void onQuit(Rml::DataModelHandle handle,
                Rml::Event& event,
                const Rml::VariantList& args);

  private:
    void onMenuBackPriv();

    con::ItemLib<Rml::ElementDocument*> rmlDocLib;
    Rml::Context* rmlContext;

    bool mouseOverUi;
    bool mouseDownInteract[3];
    bool mouseUpInteract[3];
    bool mouseWheelInteract;
    bool menuOpen;

    vector<string> menuStack;
    string currentMenuPage;
};

}  // namespace ui


#endif