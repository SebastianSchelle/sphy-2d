#ifndef MAIN_MENU_HPP
#define MAIN_MENU_HPP

#include "logging.hpp"
#include "widgets.hpp"
#include <event-listener.hpp>

namespace ui
{

struct DmMainMenu
{
    string testString = "Hello World!";
    widget::Button testButton =
        widget::Button{.id = "Test", .tooltip = "Test tooltip"};
    EventListener eventListener;

    static void RegisterType(Rml::DataModelConstructor& constructor)
    {
        widget::Button::RegisterType(constructor);
        if (auto handle = constructor.RegisterStruct<DmMainMenu>())
        {
            handle.RegisterMember("testButton", &DmMainMenu::testButton);
            handle.RegisterMember("testString", &DmMainMenu::testString);
        }
    }

    void init(Rml::DataModelConstructor& constructor)
    {
        eventListener.init(constructor);
        eventListener.createOnclick(
            testButton, [this]() { LG_D("Clicked {}", testButton.tooltip); });
    }
};

}  // namespace ui

#endif