#include "rmlui-renderinterface.hpp"
#include "std-inc.hpp"
#include "vertex-defines.hpp"

namespace gfx
{

RmlUiRenderInterface::RmlUiRenderInterface(gfx::RenderEngine* renderEngine)
    : renderEngine(renderEngine)
{
}

RmlUiRenderInterface::~RmlUiRenderInterface() {}

Rml::CompiledGeometryHandle
RmlUiRenderInterface::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                      Rml::Span<const int> indices)
{
    std::vector<VertexPosColTex> vertexData;
    vertexData.reserve(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        const auto& col = vertices[i].colour;
        uint32_t rgba = ((uint32_t)col.red) | ((uint32_t)col.green << 8)
                        | ((uint32_t)col.blue << 16)
                        | ((uint32_t)col.alpha << 24);
        vertexData.push_back({vertices[i].position.x,
                              vertices[i].position.y,
                              rgba,
                              vertices[i].tex_coord.x,
                              vertices[i].tex_coord.y});
    }
    uint32_t geometryHandle = renderEngine->compileGeometry(
        &vertexData[0],
        vertexData.size() * sizeof(VertexPosColTex),
        indices.data(),
        indices.size() * sizeof(int),
        VertexPosColTex::ms_decl,
        true);  // RmlUI uses 32-bit int indices
    return (Rml::CompiledGeometryHandle)geometryHandle;
}

void RmlUiRenderInterface::RenderGeometry(Rml::CompiledGeometryHandle geometry,
                                          Rml::Vector2f translation,
                                          Rml::TextureHandle texture)
{
    uint32_t geometryHandle = (uint32_t)geometry;
    renderEngine->renderCompiledGeometry(
        geometryHandle,
        glm::vec2(translation.x, translation.y),
        (uint32_t)texture,
        0);  // Use view 0
}

void RmlUiRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
{
    renderEngine->releaseGeometry((uint32_t)geometry);
}

Rml::TextureHandle
RmlUiRenderInterface::LoadTexture(Rml::Vector2i& texture_dimensions,
                                  const Rml::String& source)
{
    renderEngine->loadTexture(sec::uuid(),
                              "rmlui",
                              source,
                              texture_dimensions.x,
                              texture_dimensions.y);
    return 0;
}

Rml::TextureHandle
RmlUiRenderInterface::GenerateTexture(Rml::Span<const Rml::byte> source,
                                      Rml::Vector2i source_dimensions)
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
    renderEngine->enableScissor(enable);
}

void RmlUiRenderInterface::SetScissorRegion(Rml::Rectanglei region)
{
    renderEngine->setScissorRegion(region);
}

void RmlUiRenderInterface::EnableClipMask(bool enable)
{
    renderEngine->enableClipMask(enable);
}

void RmlUiRenderInterface::RenderToClipMask(
    Rml::ClipMaskOperation operation,
    Rml::CompiledGeometryHandle geometry,
    Rml::Vector2f translation)
{
    uint32_t geometryHandle = (uint32_t)geometry;
    renderEngine->renderToClipMask(
        operation, geometryHandle, glm::vec2(translation.x, translation.y), 0);
}

}  // namespace gfx