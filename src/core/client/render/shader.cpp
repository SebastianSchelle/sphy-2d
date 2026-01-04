#include "shader.hpp"

namespace gfx
{

bgfx::ShaderHandle ShaderProgram::loadShader(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        LG_E("Could not open shader file: {}", path);
        file.close();
        return BGFX_INVALID_HANDLE;
    }
    // Determine file size
    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    if (fileSize <= 0)
    {
        LG_E("Shader file is empty or unreadable: {}", path);
        file.close();
        return BGFX_INVALID_HANDLE;
    }
    file.seekg(0, std::ios::beg);
    // Allocate buffer and read contents
    std::vector<char> data(fileSize + 1);
    if (!file.read(data.data(), fileSize))
    {
        LG_E("Failed to read shader file: {}", path);
        file.close();
        return BGFX_INVALID_HANDLE;
    }
    data[fileSize] = '\0';  // null-terminate, just in case
                            // Copy to bgfx memory
    const bgfx::Memory* mem = bgfx::copy(data.data(), (uint32_t)data.size());
    bgfx::ShaderHandle handle = bgfx::createShader(mem);
    bgfx::setName(handle, path);
    LG_I("Loaded shader: {}", path);
    file.close();
    return handle;
}

ShaderProgram::ShaderProgram(const std::string& vs, const std::string& fs)
{
    vsh = loadShader(vs.c_str());
    if (!bgfx::isValid(vsh))
    {
        LG_E("Invalid vertex shader handle");
        exit(1);
    }
    fsh = loadShader(fs.c_str());
    if (!bgfx::isValid(fsh))
    {
        LG_E("Invalid fragment shader handle");
        exit(1);
    }
    program = bgfx::createProgram(vsh, fsh, true);
    if (!bgfx::isValid(program))
    {
        LG_E("Could not create shader program from {} {}", vs, fs);
        exit(1);
    }
    else
    {
        LG_I("Created shader program from {} {}", vs, fs);
    }
}

bgfx::ProgramHandle ShaderProgram::getHandle() const
{
    return program;
}

}  // namespace gfx