#ifndef UI_USER_INPUT_HPP
#define UI_USER_INPUT_HPP

#include <GLFW/glfw3.h>
#include <initializer_list>
#include <std-inc.hpp>

namespace ui
{

struct InputEvent
{
    typedef uint32_t Identifier;
    static constexpr Identifier INVALID_ID = 0xFFFFFFFF;
    enum class Type
    {
        Key,
        Scroll,
        MouseButton,
        MouseAxis,
        MouseWheel,
        GamepadButton,
        GamepadAxis,
    };
    enum class Environment
    {
        General,
        Menu,
        ThirdPerson,
        Strategic,
        Tactical,
        ModdingTools,
    };

    struct KeyData
    {
    };
    struct MouseWheelData
    {
        double offset;
    };
    struct MouseButtonData
    {
        int button;
    };
    struct MouseAxisData
    {
        float value;
    };
    typedef std::
        variant<KeyData, MouseWheelData, MouseButtonData, MouseAxisData>
            EventData;
    typedef std::function<bool(const EventData& eventData)> EventCallback;

    struct Key
    {
        static Identifier encodeIdentifier(Environment environment,
                                           uint16_t key,
                                           uint8_t modifiers,
                                           uint8_t action);
        Identifier encodeIdentifier(Environment environment) const;
        static void decodeIdentifier(Identifier identifier,
                                     Environment& environment,
                                     uint16_t& key,
                                     uint8_t& modifiers,
                                     uint8_t& action);
        void decodeIdentifier(Identifier identifier, Environment& environment);
        uint16_t key;
        uint8_t modifiers = 0;
        uint8_t action = GLFW_PRESS;
        EventCallback callback;
    };
    struct MouseWheel
    {
        Identifier createIdentifier(Environment environment) const;
        void decodeIdentifier(Identifier identifier, Environment& environment);
        uint8_t axis;
        EventCallback callback;
    };
    struct MouseButton
    {
        Identifier createIdentifier(Environment environment) const;
        void decodeIdentifier(Identifier identifier, Environment& environment);
        uint8_t action;
        EventCallback callback;
    };
    struct MouseAxis
    {
        Identifier createIdentifier(Environment environment) const;
        void decodeIdentifier(Identifier identifier, Environment& environment);
        uint8_t axis;
        EventCallback callback;
    };
    typedef std::variant<Key /*, MouseWheel, MouseButton, MouseAxis*/> Event;

    Identifier identifier = INVALID_ID;
    Identifier master = INVALID_ID;
    string name;
    string description;
    Event event;

    InputEvent(Identifier id,
               Identifier masterId,
               string name_,
               string description_,
               Event event_)
        : identifier(id), master(masterId), name(std::move(name_)),
          description(std::move(description_)), event(std::move(event_))
    {
    }
};

class UserInput
{
  public:
    UserInput();
    ~UserInput();
    InputEvent::Identifier
    addEvent(const InputEvent::Environment& environment,
             const string& name,
             const std::string& description,
             const InputEvent::Event& event,
             const InputEvent::Identifier& master = InputEvent::INVALID_ID);
    void
    addEvents(std::initializer_list<InputEvent::Environment> environments,
              const string& name,
              const std::string& description,
              const InputEvent::Event& event,
              const InputEvent::Identifier& master = InputEvent::INVALID_ID);
    void addKeyPressReleasePairs(
        std::initializer_list<InputEvent::Environment> environments,
        const string& name,
        const std::string& description,
        const uint16_t keyPress,
        const uint8_t modifiers,
        const InputEvent::EventCallback& callbackPress,
        const InputEvent::EventCallback& callbackRelease);
    bool processEvent(const InputEvent::Identifier& identifier,
                      const InputEvent::EventData& eventData);

  private:
    std::unordered_map<InputEvent::Identifier, InputEvent> events;
};

}  // namespace ui

#endif