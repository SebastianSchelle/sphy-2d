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
    GeometryHandle geometryHandle = renderEngine->compileGeometry(
        &vertexData[0],
        vertexData.size() * sizeof(VertexPosColTex),
        indices.data(),
        indices.size() * sizeof(int),
        VertexPosColTex::ms_decl,
        true);  // RmlUI uses 32-bit int indices
    return (Rml::CompiledGeometryHandle)geometryHandle.value();
}

void RmlUiRenderInterface::RenderGeometry(Rml::CompiledGeometryHandle geometry,
                                          Rml::Vector2f translation,
                                          Rml::TextureHandle texture)
{
    GeometryHandle geometryHandle((uint32_t)geometry);
    TextureHandle textureHandle((uint32_t)texture);
    renderEngine->renderCompiledGeometry(
        geometryHandle,
        glm::vec2(translation.x, translation.y),
        textureHandle,
        0);  // Use view 0
}

void RmlUiRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
{
    GeometryHandle handle((uint32_t)geometry);
    renderEngine->releaseGeometry(handle);
}

Rml::TextureHandle
RmlUiRenderInterface::LoadTexture(Rml::Vector2i& texture_dimensions,
                                  const Rml::String& source)
{
    TextureHandle textureHandle = renderEngine->loadTexture(sec::uuid(),
                                                            "rmlui",
                                                            source);
    LG_D("Texture handle passed to RmlUi: {}", textureHandle.value());
    return (Rml::TextureHandle)textureHandle.value();
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
}

void RmlUiRenderInterface::SetScissorRegion(Rml::Rectanglei region)
{
}

void RmlUiRenderInterface::EnableClipMask(bool enable)
{
}

void RmlUiRenderInterface::RenderToClipMask(
    Rml::ClipMaskOperation operation,
    Rml::CompiledGeometryHandle geometry,
    Rml::Vector2f translation)
{
}

}  // namespace gfx