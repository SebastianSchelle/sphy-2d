#ifndef VERTEX_DEFINES_HPP
#define VERTEX_DEFINES_HPP

#include "std-inc.hpp"
#include <bgfx/bgfx.h>
#include <bx/math.h>

namespace gfx
{

struct VertexPosColTex
{
    float x;
    float y;
    uint32_t rgba;
    float u;
    float v;

    static void init()
    {
        ms_decl.begin()
            .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
    }

    static bgfx::VertexLayout ms_decl;
};

struct PosVertex {
    float m_x;
    float m_y;

    static void init()
    {
        ms_decl.begin()
            .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
            .end();
    };

    static bgfx::VertexLayout ms_decl;
};

struct PosColorVertex {
    float m_x;
    float m_y;
    uint32_t m_abgr;

    static void init()
    {
        ms_decl.begin()
            .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .end();
    };

    static bgfx::VertexLayout ms_decl;
};

struct PosColorShapeVertex
{
    float offsetX, offsetY;  // Unrotated half-size offset (GPU rotates)
    float u, v;              // Local shape space (-1..1)
    uint32_t rgba;           // Color
    float shapeType;         // 0=tri,1=rect,2=circle,3=line
    float thicknessX;       // outline thickness for rect X (local), or thickness for circle
    float thicknessY;        // outline thickness for rect Y (local); separate attr so .z is reliable
    float centerX, centerY;  // Shape center (world)
    float rotationRad;       // Rotation in radians (applied on GPU)

    static void init()
    {
        ms_decl.begin()
            .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .add(bgfx::Attrib::TexCoord1, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord2, 1, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord3, 3, bgfx::AttribType::Float)
            .end();
    };

    static bgfx::VertexLayout ms_decl;
};



}  // namespace gfx

#endif  // VERTEX_DEFINES_HPP