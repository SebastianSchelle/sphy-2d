#include "rmlui-renderinterface.hpp"


namespace gfx
{

RmlUiRenderInterface::RmlUiRenderInterface(gfx::RenderEngine* renderEngine)
    : renderEngine(renderEngine)
{
}

RmlUiRenderInterface::~RmlUiRenderInterface()
{
}

Rml::CompiledGeometryHandle RmlUiRenderInterface::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices)
{
    LG_D("CompileGeometry");
    return 0;
}

void RmlUiRenderInterface::RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture)
{
    LG_D("RenderGeometry");
    if (geometry == 0) return;
}

void RmlUiRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
{
    LG_D("ReleaseGeometry");
}

Rml::TextureHandle RmlUiRenderInterface::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source)
{
    LG_D("LoadTexture");
    return 0;
}

Rml::TextureHandle RmlUiRenderInterface::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions)
{
    LG_D("GenerateTexture");
    return 0;
}

void RmlUiRenderInterface::ReleaseTexture(Rml::TextureHandle texture)
{
    LG_D("ReleaseTexture");
}

void RmlUiRenderInterface::EnableScissorRegion(bool enable)
{
    LG_D("EnableScissorRegion");
}

void RmlUiRenderInterface::SetScissorRegion(Rml::Rectanglei region)
{
    LG_D("SetScissorRegion");
}

}