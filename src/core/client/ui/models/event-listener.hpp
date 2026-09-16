#ifndef EVENT_LISTENER_HPP
#define EVENT_LISTENER_HPP

#include "RmlUi/Core/DataModelHandle.h"
#include "logging.hpp"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <std-inc.hpp>
#include <unordered_map>

namespace ui
{

typedef std::function<void()> onClickClb;

class EventListener
{
  public:
    void init(Rml::DataModelConstructor& constructor)
    {
        constructor.BindEventCallback(
            "onClick",
            [this](
                Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
            {
                if (args.empty())
                    return;

                const auto& id = args[0].Get<Rml::String>();
                if (onClickClbs.contains(id))
                {
                    onClickClbs[id]();
                }
            });
    }
    template <class T> void registerOnclick(const T& model);
    template <class T> void createOnclick(T& model, onClickClb clb);

  private:
    std::unordered_map<string, onClickClb> onClickClbs;
};

template <class T> void EventListener::registerOnclick(const T& model)
{
    if (!model.onClick)
    {
        LG_W("Could not register onClick callback. onClick is null");
        return;
    }
    if (onClickClbs.contains(model.id))
    {
        LG_W(
            "Callback {} already exists in event listener. Overwriting "
            "callback",
            model.id);
    }
    onClickClbs[model.id] = model.onClick;
}

template <class T> void EventListener::createOnclick(T& model, onClickClb clb)
{
    model.onClick = clb;
    registerOnclick(model);
}

}  // namespace ui

#endif