#ifndef SHADER_HPP
#define SHADER_HPP

#include "std-inc.hpp"
#include <bgfx/bgfx.h>
#include <bx/math.h>

namespace gfx
{
class ShaderProgram
{
  public:
    ShaderProgram(const std::string& vs, const std::string& fs);
    static bgfx::ShaderHandle loadShader(const char* _name);
    bgfx::ProgramHandle getHandle() const;

  private:
    bgfx::ShaderHandle vsh;
    bgfx::ShaderHandle fsh;
    bgfx::ProgramHandle program;
};

}  // namespace gfx

#endif