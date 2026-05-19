#ifndef UI_USER_INPUT_HPP
#define UI_USER_INPUT_HPP

#include <std-inc.hpp>

namespace ui
{

struct InputEvent
{
    typedef uint32_t Identifier;
    static constexpr Identifier MASTER_NONE = 0xFFFFFFFF;
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
        uint32_t master = MASTER_NONE;
        uint16_t key;
        uint8_t modifiers;
        uint8_t action;
        EventCallback callback;
    };
    struct MouseWheel
    {
        Identifier createIdentifier(Environment environment) const;
        void decodeIdentifier(Identifier identifier, Environment& environment);
        uint32_t master = MASTER_NONE;
        uint8_t axis;
        EventCallback callback;
    };
    struct MouseButton
    {
        Identifier createIdentifier(Environment environment) const;
        void decodeIdentifier(Identifier identifier, Environment& environment);
        uint32_t master = MASTER_NONE;
        uint8_t action;
        EventCallback callback;
    };
    struct MouseAxis
    {
        Identifier createIdentifier(Environment environment) const;
        void decodeIdentifier(Identifier identifier, Environment& environment);
        uint32_t master = MASTER_NONE;
        uint8_t axis;
        EventCallback callback;
    };
    typedef std::variant<Key /*, MouseWheel, MouseButton, MouseAxis*/> Event;

    Identifier identifier;
    string name;
    string description;
    Event event;
};

class UserInput
{
  public:
    UserInput();
    ~UserInput();
    void addEvent(const InputEvent::Environment& environment,
                  const string& name,
                  const std::string& description,
                  const InputEvent::Event& event);
    bool processEvent(const InputEvent::Identifier& identifier,
                      const InputEvent::EventData& eventData);

  private:
    std::unordered_map<InputEvent::Identifier, InputEvent> events;
};

}  // namespace ui

#endif