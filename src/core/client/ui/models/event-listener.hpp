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
typedef std::function<void()> onChangeClb;

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
                if (auto it = onClickClbs.find(id); it != onClickClbs.end())
                    it->second();
            });
        constructor.BindEventCallback(
            "onChange",
            [this](
                Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
            {
                if (args.empty())
                    return;

                const auto& id = args[0].Get<Rml::String>();
                if (auto it = onChangeClbs.find(id); it != onChangeClbs.end())
                    it->second();
            });
    }
    template <class T> void registerOnclick(const T& model);
    template <class T> void createOnclick(T& model, onClickClb clb);
    template <class T> void registerOnchange(const T& model);
    template <class T> void createOnchange(T& model, onChangeClb clb);

  private:
    std::unordered_map<string, onClickClb> onClickClbs;
    std::unordered_map<string, onChangeClb> onChangeClbs;
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

template <class T> void EventListener::registerOnchange(const T& model)
{
    if (!model.onChange)
    {
        LG_W("Could not register onChange callback. onChange is null");
        return;
    }
    if (onChangeClbs.contains(model.id))
    {
        LG_W(
            "Callback {} already exists in event listener. Overwriting "
            "callback",
            model.id);
    }
    onChangeClbs[model.id] = model.onChange;
}

template <class T> void EventListener::createOnchange(T& model, onChangeClb clb)
{
    model.onChange = clb;
    registerOnchange(model);
}

struct DataModel
{
  protected:
    EventListener eventListener;
    Rml::DataModelHandle rmlHandle;

};

}  // namespace ui

#endif