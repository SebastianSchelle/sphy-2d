#ifndef SHADER_HPP
#define SHADER_HPP

#include "std-inc.hpp"
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <item-lib.hpp>

namespace gfx
{
class ShaderProgram
{
  public:
    ShaderProgram(const std::string& vs, const std::string& fs);
    static bgfx::ShaderHandle loadShader(const char* _name);
    bgfx::ProgramHandle getHandle() const;
    void destroy() const;

  private:
    bgfx::ProgramHandle program;
};

using ShaderHandle = typename con::ItemLib<ShaderProgram>::Handle;

}  // namespace gfx

#endif