#ifndef WIDGETS_HPP
#define WIDGETS_HPP

#include "RmlUi/Core/DataModelHandle.h"
#include "event-listener.hpp"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <algorithm>
#include <std-inc.hpp>
#include <unordered_map>

namespace ui
{
namespace widget
{

struct Button
{
    string id;
    string label;
    string tooltip;
    bool disabled = false;
    OnClickClb onClick;

    static void RegisterType(Rml::DataModelConstructor& constructor)
    {
        if (auto handle = constructor.RegisterStruct<Button>())
        {
            handle.RegisterMember("id", &Button::id);
            handle.RegisterMember("label", &Button::label);
            handle.RegisterMember("tooltip", &Button::tooltip);
            handle.RegisterMember("disabled", &Button::disabled);
        }
    }
};

struct Checkbox
{
    string id;
    string label;
    string tooltip;
    bool checked = false;
    bool disabled = false;
    OnChangeClb onChange;

    static void RegisterType(Rml::DataModelConstructor& constructor)
    {
        if (auto handle = constructor.RegisterStruct<Checkbox>())
        {
            handle.RegisterMember("id", &Checkbox::id);
            handle.RegisterMember("label", &Checkbox::label);
            handle.RegisterMember("tooltip", &Checkbox::tooltip);
            handle.RegisterMember("checked", &Checkbox::checked);
            handle.RegisterMember("disabled", &Checkbox::disabled);
        }
    }
};

template <class T> struct RadioOption
{
    T value;
    string valStr;
    string label;
    string tooltip;

    static void RegisterType(Rml::DataModelConstructor& constructor)
    {
        if (auto handle = constructor.RegisterStruct<RadioOption>())
        {
            handle.RegisterMember("label", &RadioOption::label);
            handle.RegisterMember("value", &RadioOption::valStr);
            handle.RegisterMember("tooltip", &RadioOption::tooltip);
        }
    }
};

template <class T> struct RadioButtons
{
    string id;
    string label;
    string tooltip;
    vector<RadioOption<T>> options;
    string value;
    bool disabled = false;
    unordered_map<string, T> values;
    OnChangeClb onChange;

    void initOptions()
    {
        values.clear();
        values.reserve(options.size());
        for (auto& option : options)
        {
            values.emplace(option.valStr, option.value);
        }
    }

    T getSelectedValue()
    {
        if (auto it = values.find(value); it != values.end())
        {
            return it->second;
        }
        return T{};
    }

    static void RegisterType(Rml::DataModelConstructor& constructor)
    {
        RadioOption<T>::RegisterType(constructor);
        constructor.RegisterArray<std::vector<RadioOption<T>>>();
        if (auto handle = constructor.RegisterStruct<RadioButtons>())
        {
            handle.RegisterMember("id", &RadioButtons::id);
            handle.RegisterMember("label", &RadioButtons::label);
            handle.RegisterMember("tooltip", &RadioButtons::tooltip);
            handle.RegisterMember("options", &RadioButtons::options);
            handle.RegisterMember("disabled", &RadioButtons::disabled);
            handle.RegisterMember("value", &RadioButtons::value);
        }
    }
};


struct Window
{
    string title;
    bool movable;
    bool closable;

    static void RegisterType(Rml::DataModelConstructor& constructor)
    {
        if (auto handle = constructor.RegisterStruct<Window>())
        {
            handle.RegisterMember("title", &Window::title);
            handle.RegisterMember("movable", &Window::movable);
            handle.RegisterMember("closable", &Window::closable);
        }
    }

};


}  // namespace widget
}  // namespace ui

#endif