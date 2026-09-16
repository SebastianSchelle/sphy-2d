#ifndef RMLUI_SYSTEMINTERFACE_HPP
#define RMLUI_SYSTEMINTERFACE_HPP

#include "RmlUi/Core/SystemInterface.h"
#include "ptr-handle.hpp"
#include <std-inc.hpp>

namespace ui
{

class RmlUiSystemInterface : public Rml::SystemInterface
{
  public:
    RmlUiSystemInterface(ecs::PtrHandle* ptrHandle);
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;
    int TranslateString(Rml::String& translated,
                        const Rml::String& input) override;

  private:
    ecs::PtrHandle* ptrHandle;
};

}  // namespace ui

#endif