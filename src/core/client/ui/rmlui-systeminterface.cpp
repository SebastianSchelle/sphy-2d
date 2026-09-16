#include "rmlui-systeminterface.hpp"
#include "logging.hpp"
#include "ptr-handle.hpp"

namespace ui
{

RmlUiSystemInterface::RmlUiSystemInterface(ecs::PtrHandle* ptrHandle)
    : ptrHandle(ptrHandle)
{
}

int RmlUiSystemInterface::TranslateString(Rml::String& translated,
                                          const Rml::String& input)
{
    ptrHandle->locale->translateTokenString(translated, input);
    return 0;
}

bool RmlUiSystemInterface::LogMessage(Rml::Log::Type type,
                                      const Rml::String& message)
{
    switch (type)
    {
        case Rml::Log::Type::LT_ERROR:
            LG_E(message);
            break;
        case Rml::Log::Type::LT_WARNING:
            LG_W(message);
            break;
        case Rml::Log::Type::LT_INFO:
            LG_I(message);
            break;
        case Rml::Log::Type::LT_DEBUG:
            LG_D(message);
            break;
        default:
            break;
    }
    return true;
}

}  // namespace ui