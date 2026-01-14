#ifndef RMLUI_SYSTEMINTERFACE_HPP
#define RMLUI_SYSTEMINTERFACE_HPP

#include "RmlUi/Core/SystemInterface.h"

namespace ui
{

class RmlUiSystemInterface : public Rml::SystemInterface
{
  public:
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;
};

}  // namespace ui

#endif