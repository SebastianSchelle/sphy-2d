#ifndef WIDGETS_HPP
#define WIDGETS_HPP

#include "RmlUi/Core/DataModelHandle.h"
#include "event-listener.hpp"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <std-inc.hpp>

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
    onClickClb onClick;

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
    onChangeClb onChange;

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


}  // namespace widget
}  // namespace ui

#endif