#ifndef MAIN_MENU_HPP
#define MAIN_MENU_HPP

#include "logging.hpp"
#include "widgets.hpp"
#include <event-listener.hpp>

namespace ui
{

struct DmMainMenu : public DataModel
{
    string title = "[menu.main.title]";
    widget::Button testButton = widget::Button{.id = "test",
                                               .label = "Test",
                                               .tooltip = "Test tooltip"};
    widget::Button testButton2 = widget::Button{.id = "test2",
                                                .label = "Test2",
                                                .tooltip = "Test tooltip 2"};
    widget::Checkbox testCheckbox =
        widget::Checkbox{.id = "check-test",
                         .label = "Check test",
                         .tooltip = "Check test tooltip",
                         .checked = true};

    widget::Checkbox testCheckbox2 =
        widget::Checkbox{.id = "check-test-2",
                         .label = "Check test 2",
                         .tooltip = "Check test tooltip 2"};

    static void RegisterType(Rml::DataModelConstructor& constructor)
    {
        widget::Button::RegisterType(constructor);
        widget::Checkbox::RegisterType(constructor);

        if (auto handle = constructor.RegisterStruct<DmMainMenu>())
        {
            handle.RegisterMember("testButton", &DmMainMenu::testButton);
            handle.RegisterMember("testButton2", &DmMainMenu::testButton2);
            handle.RegisterMember("testCheckbox", &DmMainMenu::testCheckbox);
            handle.RegisterMember("testCheckbox2", &DmMainMenu::testCheckbox2);
            handle.RegisterMember("title", &DmMainMenu::title);
        }
    }

    void init(Rml::DataModelConstructor& constructor,
              Rml::DataModelHandle rmlHdl)
    {
        this->rmlHandle = rmlHdl;
        eventListener.init(constructor);
        eventListener.createOnclick(
            testButton, [this]() { LG_D("Clicked {}", testButton.tooltip); });
        eventListener.createOnclick(
            testButton2, [this]() { LG_D("Clicked {}", testButton2.tooltip); });
        eventListener.createOnchange(testCheckbox,
                                     [this]()
                                     {
                                         LG_D("State of {} changed to {}",
                                              testCheckbox.id,
                                              testCheckbox.checked);
                                         testButton.disabled =
                                             !testCheckbox.checked;
                                         testCheckbox2.disabled =
                                             !testCheckbox.checked;
                                     });
    }
};

}  // namespace ui

#endif