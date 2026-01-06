#ifndef USER_INTERFACE_HPP
#define USER_INTERFACE_HPP

#include <item-lib.hpp>
#include "RmlUi/Core/Core.h"
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Context.h>

using UiDocHandle = con::ItemLib<Rml::ElementDocument*>::Handle;

namespace ui
{

class UserInterface
{
  public:
    UserInterface();
    ~UserInterface();
    bool init(glm::ivec2 windowSize);
    void updateContext(glm::ivec2 windowSize);
    void processMouseMove(glm::ivec2 mousePos, int keyMod);
    void processMouseButtonDown(int button, int keyMod);
    void processMouseButtonUp(int button, int keyMod);
    bool isMouseInteracting();
    void processMouseWheel(int delta, int keyMod);
    void render();

    UiDocHandle loadDocument(const std::string& name, const std::string& documentPath);
    bool loadFont(const std::string& fontPath);

private:
    con::ItemLib<Rml::ElementDocument*> rmlDocLib;
    Rml::Context* rmlContext;

    bool mouseOverUi;
    bool mouseDownInteract[3];
    bool mouseUpInteract[3];
    bool mouseWheelInteract;
};

}  // namespace ui


#endif