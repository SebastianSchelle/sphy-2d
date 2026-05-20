#include "user-input.hpp"

namespace ui
{


InputEvent::Identifier
InputEvent::Key::encodeIdentifier(Environment environment,
                                  uint16_t key,
                                  uint8_t modifiers,
                                  uint8_t action)
{
    return Identifier(
        (static_cast<uint32_t>(environment) & 0x3f) |       // 6 bit environment
        ((static_cast<uint32_t>(Type::Key) & 0x1f) << 6) |  // 5 bits for type
        ((key & 0x1ff) << 11) |                             // 9 bits for key
        ((modifiers & 0x3f) << 20) |  // 6 bits for modifiers
        ((action & 0x3) << 26));      // 2 bits for action
}

InputEvent::Identifier
InputEvent::Key::encodeIdentifier(const Environment environment) const
{
    return encodeIdentifier(environment, key, modifiers, action);
}

void InputEvent::Key::decodeIdentifier(Identifier identifier,
                                       Environment& environment,
                                       uint16_t& key,
                                       uint8_t& modifiers,
                                       uint8_t& action)
{
    environment = static_cast<Environment>(identifier & 0x3f);
    key = (identifier >> 11) & 0x1ff;
    modifiers = (identifier >> 20) & 0x3f;
    action = (identifier >> 26) & 0x3;
}

void InputEvent::Key::decodeIdentifier(Identifier identifier,
                                       Environment& environment)
{
    decodeIdentifier(identifier, environment, key, modifiers, action);
}

UserInput::UserInput() {}

UserInput::~UserInput() {}

InputEvent::Identifier
UserInput::addEvent(const InputEvent::Environment& environment,
                    const string& name,
                    const std::string& description,
                    const InputEvent::Event& event,
                    const InputEvent::Identifier& master)
{
    InputEvent::Identifier identifier;
    std::visit([&identifier, environment](const auto& event)
               { identifier = event.encodeIdentifier(environment); },
               event);
    events.emplace(std::piecewise_construct,
                   std::forward_as_tuple(identifier),
                   std::forward_as_tuple(identifier,
                                         master,
                                         name,
                                         description,
                                         event));
    LG_D("Added event: {}:{}", identifier, name);
    return identifier;
}

void UserInput::addEvents(std::initializer_list<InputEvent::Environment> environments,
                          const string& name,
                          const std::string& description,
                          const InputEvent::Event& event,
                          const InputEvent::Identifier& master)
{
    for (const auto& environment : environments)
    {
        addEvent(environment, name, description, event, master);
    }
}

void UserInput::addKeyPressReleasePairs(
    std::initializer_list<InputEvent::Environment> environments,
    const string& name,
    const std::string& description,
    const uint16_t keyPress,
    const uint8_t modifiers,
    const InputEvent::EventCallback& callbackPress,
    const InputEvent::EventCallback& callbackRelease)
{
    for (const auto& environment : environments)
    {
        InputEvent::Identifier masterId =
            addEvent(environment,
                     name,
                     description,
                     InputEvent::Key{.key = keyPress,
                                     .modifiers = modifiers,
                                     .action = GLFW_PRESS,
                                     .callback = callbackPress},
                     InputEvent::INVALID_ID);
        addEvent(environment,
                 "Stop: " + name,
                 "Stop: " + description,
                 InputEvent::Key{.key = keyPress,
                                 .modifiers = modifiers,
                                 .action = GLFW_RELEASE,
                                 .callback = callbackRelease},
                 masterId);
    }
}

bool UserInput::processEvent(const InputEvent::Identifier& identifier,
                             const InputEvent::EventData& eventData)
{
    auto it = events.find(identifier);
    LG_D("Processing event: {}", identifier);
    if (it == events.end())
    {
        return false;
    }
    return std::visit([&eventData, &it](const auto& event)
                      { return event.callback(eventData); },
                      it->second.event);
}

}  // namespace ui