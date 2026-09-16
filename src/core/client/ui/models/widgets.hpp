#ifndef WIDGETS_HPP
#define WIDGETS_HPP

#include "RmlUi/Core/DataModelHandle.h"
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
    string tooltip;
    bool en = true;
    std::function<void()> onClick;

    static void RegisterType(Rml::DataModelConstructor& constructor)
    {
        if (auto handle = constructor.RegisterStruct<Button>())
        {
            handle.RegisterMember("id", &Button::id);
            handle.RegisterMember("tooltip", &Button::tooltip);
            handle.RegisterMember("en", &Button::en);
        }
    }
};

}  // namespace widget
}  // namespace ui

#endif