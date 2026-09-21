#ifndef EVENT_LISTENER_HPP
#define EVENT_LISTENER_HPP

#include "RmlUi/Core/DataModelHandle.h"
#include "RmlUi/Core/EventListener.h"
#include "logging.hpp"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <std-inc.hpp>
#include <unordered_map>

namespace ui
{

typedef std::function<void()> OnClickClb;
typedef std::function<void()> OnChangeClb;
typedef std::function<void()> OnCloseClb;

struct EventFunctions
{
    OnCloseClb onClose = nullptr;
};

class EventListener
{
  public:
    void init(Rml::DataModelConstructor& constructor,
              const EventFunctions& eventFunctions)
    {
        if (eventFunctions.onClose)
        {
            constructor.BindEventCallback(
                "onCloseDocument",
                [this, eventFunctions](Rml::DataModelHandle,
                                       Rml::Event&,
                                       const Rml::VariantList& args)
                { eventFunctions.onClose(); });
        }
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
    template <class T> void createOnclick(T& model, OnClickClb clb);
    template <class T> void registerOnchange(const T& model);
    template <class T> void createOnchange(T& model, OnChangeClb clb);

  private:
    std::unordered_map<string, OnClickClb> onClickClbs;
    std::unordered_map<string, OnChangeClb> onChangeClbs;
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

template <class T> void EventListener::createOnclick(T& model, OnClickClb clb)
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

template <class T> void EventListener::createOnchange(T& model, OnChangeClb clb)
{
    model.onChange = clb;
    registerOnchange(model);
}

class PageEventListener : public Rml::EventListener
{
  public:
    typedef std::function<void()> OnShow;
    struct EventClbs
    {
        OnShow onShow = nullptr;
    };
    PageEventListener(EventClbs clbs) : clbs(clbs) {}

    void ProcessEvent(Rml::Event& event) override
    {
        if (event.GetType() == "show")
            if (clbs.onShow)
                clbs.onShow();
    }

  private:
    EventClbs clbs;
};

struct DataModel
{
  protected:
    EventListener eventListener;
    Rml::DataModelHandle rmlHandle;
    void setup(Rml::DataModelConstructor& constructor,
               Rml::DataModelHandle rmlHdl,
               const EventFunctions& eventFunctions)
    {
        this->rmlHandle = rmlHdl;
        eventListener.init(constructor, eventFunctions);
    }
};

}  // namespace ui

#endif