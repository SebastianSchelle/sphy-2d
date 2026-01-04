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

}  // namespace gfx

#endif  // VERTEX_DEFINES_HPP