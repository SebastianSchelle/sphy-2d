#include "rmlui-renderinterface.hpp"
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
    // LG_D("Compile Geometry vertices: {}, indices: {}",
    //      vertices.size(),
    //      indices.size());
    std::vector<VertexPosColTex> vertexData(vertices.size());
    for (int i = 0; i < vertices.size(); ++i)
    {
        vertexData.push_back({vertices[i].position.x,
                              vertices[i].position.y,
                              (uint32_t)((uint8_t)vertices[i].colour.red << 24 | (uint8_t)vertices[i].colour.green << 16 | (uint8_t)vertices[i].colour.blue << 8 | (uint8_t)vertices[i].colour.alpha),
                              vertices[i].tex_coord.x,
                              vertices[i].tex_coord.y});
    }
    uint32_t geometryHandle =
        renderEngine->compileGeometry(&vertexData[0],
                                      vertexData.size(),
                                      indices.data(),
                                      indices.size(),
                                      VertexPosColTex::ms_decl);
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
        (uint32_t)texture);
}

void RmlUiRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
{
    renderEngine->releaseGeometry((uint32_t)geometry);
}

Rml::TextureHandle
RmlUiRenderInterface::LoadTexture(Rml::Vector2i& texture_dimensions,
                                  const Rml::String& source)
{
    //LG_D("LoadTexture");
    return 0;
}

Rml::TextureHandle
RmlUiRenderInterface::GenerateTexture(Rml::Span<const Rml::byte> source,
                                      Rml::Vector2i source_dimensions)
{
    //LG_D("GenerateTexture");
    return 0;
}

void RmlUiRenderInterface::ReleaseTexture(Rml::TextureHandle texture)
{
    //LG_D("ReleaseTexture");
}

void RmlUiRenderInterface::EnableScissorRegion(bool enable)
{
    //LG_D("EnableScissorRegion");
}

void RmlUiRenderInterface::SetScissorRegion(Rml::Rectanglei region)
{
    //LG_D("SetScissorRegion");
}

}  // namespace gfx