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
    glm::ivec2 textureSize = renderEngine->getTextureSize();
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
                              (float)vertices[i].tex_coord.x,
                              (float)vertices[i].tex_coord.y});
    }

    GeometryHandle geometryHandle = renderEngine->compileGeometry(
        &vertexData[0],
        vertexData.size() * sizeof(VertexPosColTex),
        indices.data(),
        indices.size() * sizeof(int),
        VertexPosColTex::ms_decl,
        true);  // RmlUI uses 32-bit int indices

    // if (geometryHandle.isValid())
    // {
    //     LG_D("RmlUi Geometry created with handle idx: {}",
    //          geometryHandle.getIdx());
    //     for (int i = 0; i < (int)indices.size(); ++i)
    //     {
    //         LG_D("{}: pos:({}, {}) uv:({}, {})",
    //              indices[i],
    //              vertexData[indices[i]].x,
    //              vertexData[indices[i]].y,
    //              vertexData[indices[i]].u,
    //              vertexData[indices[i]].v);
    //         if ((i - 2) % 3 == 0)
    //             LG_D("");
    //     }
    // }

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
        1);  // Use view 1 (UI layer)
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
    LG_D("LoadTexture: source='{}'", source);
    glm::vec2 dimensions;
    TextureHandle textureHandle =
        renderEngine->loadTexture(sec::uuid(), "rmlui", source, dimensions);
    texture_dimensions.x = dimensions.x;
    texture_dimensions.y = dimensions.y;
    return (Rml::TextureHandle)textureHandle.value();
}

Rml::TextureHandle
RmlUiRenderInterface::GenerateTexture(Rml::Span<const Rml::byte> source,
                                      Rml::Vector2i source_dimensions)
{
    TextureHandle textureHandle =
        renderEngine->generateTexture(sec::uuid(),
                                      "rmlui",
                                      source.data(),
                                      source_dimensions.x,
                                      source_dimensions.y);
    return (Rml::TextureHandle)textureHandle.value();
}

void RmlUiRenderInterface::ReleaseTexture(Rml::TextureHandle texture)
{
    LG_D("ReleaseTexture");
}

void RmlUiRenderInterface::EnableScissorRegion(bool enable)
{
    renderEngine->enableScissorRegion(enable);
}

void RmlUiRenderInterface::SetScissorRegion(Rml::Rectanglei region)
{
    renderEngine->setScissorRegion(glm::vec2(region.Left(), region.Top()),
                                   glm::vec2(region.Width(),
                                   region.Height()));
}

void RmlUiRenderInterface::EnableClipMask(bool enable)
{
    //LG_D("EnableClipMask");
}

void RmlUiRenderInterface::RenderToClipMask(
    Rml::ClipMaskOperation operation,
    Rml::CompiledGeometryHandle geometry,
    Rml::Vector2f translation)
{
    //LG_D("RenderToClipMask");
}

void RmlUiRenderInterface::SetTransform(const Rml::Matrix4f* transform)
{
    glm::mat4 transformMatrix;
    
    if (transform)
    {
        // Rml::Matrix4f is column-major, same as glm::mat4
        // Copy the 16 float values directly (both use column-major storage)
        const float* data = transform->data();
        // glm::mat4 stores data column-major: [col0, col1, col2, col3]
        // where each column is [x, y, z, w]
        transformMatrix = glm::mat4(
            data[0],  data[1],  data[2],  data[3],   // column 0
            data[4],  data[5],  data[6],  data[7],   // column 1
            data[8],  data[9],  data[10], data[11],  // column 2
            data[12], data[13], data[14], data[15]   // column 3
        );
    }
    else
    {
        // Set identity matrix if transform is nullptr
        transformMatrix = glm::mat4(1.0f);
    }
    
    renderEngine->setTransform(transformMatrix);
}

Rml::LayerHandle RmlUiRenderInterface::PushLayer()
{
    LG_D("PushLayer");
    return Rml::LayerHandle{};
}

void RmlUiRenderInterface::CompositeLayers(
    Rml::LayerHandle source,
    Rml::LayerHandle destination,
    Rml::BlendMode blend_mode,
    Rml::Span<const Rml::CompiledFilterHandle> filters)
{
    LG_D("CompositeLayers");
}

void RmlUiRenderInterface::PopLayer()
{
    LG_D("PopLayer");
}

Rml::TextureHandle RmlUiRenderInterface::SaveLayerAsTexture()
{
    LG_D("SaveLayerAsTexture");
    return Rml::TextureHandle{};
}

Rml::CompiledFilterHandle RmlUiRenderInterface::SaveLayerAsMaskImage()
{
    LG_D("SaveLayerAsMaskImage");
    return Rml::CompiledFilterHandle{};
}

Rml::CompiledFilterHandle
RmlUiRenderInterface::CompileFilter(const Rml::String& name,
                                    const Rml::Dictionary& parameters)
{
    LG_D("CompileFilter");
    return Rml::CompiledFilterHandle{};
}

void RmlUiRenderInterface::ReleaseFilter(Rml::CompiledFilterHandle filter)
{
    LG_D("ReleaseFilter");
}

Rml::CompiledShaderHandle
RmlUiRenderInterface::CompileShader(const Rml::String& name,
                                    const Rml::Dictionary& parameters)
{
    LG_D("CompileShader");
    return Rml::CompiledShaderHandle{};
}

void RmlUiRenderInterface::RenderShader(Rml::CompiledShaderHandle shader,
                                        Rml::CompiledGeometryHandle geometry,
                                        Rml::Vector2f translation,
                                        Rml::TextureHandle texture)
{
    LG_D("RenderShader");
}

void RmlUiRenderInterface::ReleaseShader(Rml::CompiledShaderHandle shader)
{
    LG_D("ReleaseShader");
}

}  // namespace gfx