#ifndef DM_WINDOW_HPP
#define DM_WINDOW_HPP

#include "widgets.hpp"
#include <client.hpp>
#include <event-listener.hpp>

namespace ui
{

template <class T> struct DmWindow : public DataModel
{
    string title;
    bool movable;
    bool closable;
    T data;
    std::vector<widget::Button> buttons;

    static void RegisterType(Rml::DataModelConstructor& constructor)
    {
        T::RegisterType(constructor);
        widget::Button::RegisterType(constructor);
        widget::Checkbox::RegisterType(constructor);
        widget::RadioButtons<int>::RegisterType(constructor);

        if (auto handle = constructor.RegisterStruct<DmWindow>())
        {
            handle.RegisterMember("title", &DmWindow<T>::title);
            handle.RegisterMember("movable", &DmWindow<T>::movable);
            handle.RegisterMember("closable", &DmWindow<T>::closable);
            handle.RegisterMember("data", &DmWindow<T>::data);
        }
    }

    void setup(Rml::DataModelConstructor& constructor,
               Rml::DataModelHandle rmlHdl,
               const EventFunctions& eventFunctions)
    {
        constructor.Bind("win", this);
        for (auto& b : buttons)
        {
            constructor.Bind(b.id, &b);
        }
        DataModel::setup(constructor, rmlHdl, eventFunctions);
    }

    void addButton(Rml::DataModelConstructor& constructor,
                   const widget::Button& button,
                   OnClickClb onClick)
    {
        buttons.push_back(button);
        auto& b = buttons.back();
        eventListener.createOnclick(b, onClick);
    }
};

struct DmNone
{
    static void RegisterType(Rml::DataModelConstructor& constructor) {}
};

/*
struct DmTest : public DataModel
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
                       .closable = false};

    static void RegisterType(Rml::DataModelConstructor& constructor)
    {
        widget::Button::RegisterType(constructor);
        widget::Checkbox::RegisterType(constructor);
        widget::RadioButtons<int>::RegisterType(constructor);
        widget::Window::RegisterType(constructor);

        if (auto handle = constructor.RegisterStruct<DmTest>())
        {
            handle.RegisterMember("testButton", &DmTest::testButton);
            handle.RegisterMember("testButton2", &DmTest::testButton2);
            handle.RegisterMember("testCheckbox", &DmTest::testCheckbox);
            handle.RegisterMember("testCheckbox2", &DmTest::testCheckbox2);
            handle.RegisterMember("radio", &DmTest::radioButtons);
            handle.RegisterMember("win", &DmTest::win);
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
                                        if (!process.Start("ls", {}))
                                        {
                                            LG_E("ls could not be run");
                                            return;
                                        }
                                        int exitCode = process.Wait();
                                        if (exitCode != 0)
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
*/

}  // namespace ui

#endif