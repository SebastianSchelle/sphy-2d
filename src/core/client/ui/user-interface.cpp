#include "user-interface.hpp"
#include "RmlUi/Core/DataModelHandle.h"
#include "RmlUi/Core/PropertySpecification.h"

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

    return true;
}

void UserInterface::update()
{
    rmlContext->Update();
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
        if (menuOpen) {
            if (!keepMenuOpen) {
                closeMenu();
            }
        } else {
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

}  // namespace ui

template class con::ItemLib<Rml::ElementDocument*>;