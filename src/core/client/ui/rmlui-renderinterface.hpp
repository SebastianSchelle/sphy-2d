#ifndef RMLUI_RENDERINTERFACE_HPP
#define RMLUI_RENDERINTERFACE_HPP

#include "RmlUi/Core.h"
#include "std-inc.hpp"
#include "render-engine.hpp"

namespace gfx
{

class RmlUiRenderInterface : public Rml::RenderInterface
{
  public:
    RmlUiRenderInterface(gfx::RenderEngine* renderEngine);
    ~RmlUiRenderInterface();
    Rml::CompiledGeometryHandle
    CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                    Rml::Span<const int> indices) override;
    void RenderGeometry(Rml::CompiledGeometryHandle geometry,
                        Rml::Vector2f translation,
                        Rml::TextureHandle texture) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;
    Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions,
                                   const Rml::String& source) override;
    Rml::TextureHandle
    GenerateTexture(Rml::Span<const Rml::byte> source,
                    Rml::Vector2i source_dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture) override;
    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;
    void EnableClipMask(bool enable) override;
    void RenderToClipMask(Rml::ClipMaskOperation operation,
                          Rml::CompiledGeometryHandle geometry,
                          Rml::Vector2f translation) override;

  private:
    gfx::RenderEngine* renderEngine;
};

}  // namespace gfx

#endif