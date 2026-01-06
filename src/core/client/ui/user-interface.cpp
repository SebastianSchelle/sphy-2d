#include "user-interface.hpp"

namespace ui
{

UserInterface::UserInterface() {}

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
    if(!Rml::LoadFontFace("modules/engine/assets/fonts/Orbitron-Regular.ttf"))
    {
        LG_E("Failed to load default font");
        return false;
    }
    // Load mod loading ui. This UI is always needed from the start.
    UiDocHandle modLoadingHandle = loadDocument("mod-loading", "modules/engine/assets/ui/mod-loading.rml");
    if(!modLoadingHandle.isValid())
    {
        LG_E("Failed to load mod loading ui");
        return false;
    }

    return true;
}

void UserInterface::processMouseMove(glm::ivec2 mousePos, int keyMod)
{
    mouseOverUi = !rmlContext->ProcessMouseMove(mousePos.x, mousePos.y, keyMod);
}

void UserInterface::processMouseButtonDown(int button, int keyMod)
{
    if(button >= 3)
    {
        LG_E("Button index out of range");
        return;
    }
    mouseDownInteract[button] = !rmlContext->ProcessMouseButtonDown(button, keyMod);
}

void UserInterface::processMouseButtonUp(int button, int keyMod)
{
    if(button >= 3)
    {
        LG_E("Button index out of range");
        return;
    }
    mouseUpInteract[button] = !rmlContext->ProcessMouseButtonUp(button, keyMod);
}

bool UserInterface::isMouseInteracting()
{
    return rmlContext->IsMouseInteracting();
}

void UserInterface::processMouseWheel(int delta, int keyMod)
{
    mouseWheelInteract = !rmlContext->ProcessMouseWheel(delta, keyMod);
}

void UserInterface::updateContext(glm::ivec2 windowSize)
{
    rmlContext->SetDimensions(Rml::Vector2i(windowSize.x, windowSize.y));
}

void UserInterface::render()
{
    rmlContext->Render();
}

UiDocHandle UserInterface::loadDocument(const std::string& name, const std::string& documentPath)
{
    Rml::ElementDocument* document = rmlContext->LoadDocument(documentPath);
    if (!document)
    {
        return UiDocHandle::Invalid();
    }
    UiDocHandle handle = rmlDocLib.addItem(name, document);

    return handle;
}

bool UserInterface::loadFont(const std::string& fontPath)
{
    return Rml::LoadFontFace(fontPath);
}

}  // namespace ui

template class con::ItemLib<Rml::ElementDocument*>;