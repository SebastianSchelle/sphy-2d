#ifndef MAIN_MENU_HPP
#define MAIN_MENU_HPP

#include "logging.hpp"
#include "process.hpp"
#include "widgets.hpp"
#include <event-listener.hpp>

namespace ui
{

struct DmMainMenu : public DataModel
{
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

    widget::RadioButtons<int> radioButtons{.id = "radio-buttons",
                                           .label = "Test Radio Button",
                                           .tooltip = "Radio test tooltip",
                                           .options =
                                               {
                                                   {.value = 0,
                                                    .valStr = "cat",
                                                    .label = "Cat",
                                                    .tooltip = "That's a cat"},
                                                   {.value = 1,
                                                    .valStr = "dog",
                                                    .label = "Dog",
                                                    .tooltip = "That's a dog"},
                                               },
                                           .value = "cat"};

    widget::Window win{.title = "[menu.main.title]",
                       .movable = false,
                       .closable = true};

    static void RegisterType(Rml::DataModelConstructor& constructor)
    {
        widget::Button::RegisterType(constructor);
        widget::Checkbox::RegisterType(constructor);
        widget::RadioButtons<int>::RegisterType(constructor);
        widget::Window::RegisterType(constructor);

        if (auto handle = constructor.RegisterStruct<DmMainMenu>())
        {
            handle.RegisterMember("testButton", &DmMainMenu::testButton);
            handle.RegisterMember("testButton2", &DmMainMenu::testButton2);
            handle.RegisterMember("testCheckbox", &DmMainMenu::testCheckbox);
            handle.RegisterMember("testCheckbox2", &DmMainMenu::testCheckbox2);
            handle.RegisterMember("radio", &DmMainMenu::radioButtons);
            handle.RegisterMember("win", &DmMainMenu::win);
        }
    }

    void init(Rml::DataModelConstructor& constructor,
              Rml::DataModelHandle rmlHdl,
              const EventFunctions& eventFunctions)
    {
        radioButtons.initOptions();
        DataModel::init(constructor, rmlHdl, eventFunctions);
        eventListener.createOnclick(testButton,
                                    [this]()
                                    {
                                        LG_D("Start Process");
                                        osh::Process process;
                                        if(!process.Start("ls", {}))
                                        {
                                            LG_E("ls could not be run");
                                            return;
                                        }
                                        int exitCode = process.Wait();
                                        if(exitCode != 0)
                                        {
                                            LG_E("ls failed");
                                        }
                                    });
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
        eventListener.createOnchange(radioButtons,
                                     [this]()
                                     {
                                         LG_D("State of {} changed to {}",
                                              radioButtons.id,
                                              radioButtons.getSelectedValue());
                                     });
    }
};

}  // namespace ui

#endif