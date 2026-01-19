#ifndef LUA_INTERPRETER_HPP
#define LUA_INTERPRETER_HPP

#include <sol/sol.hpp>

namespace mod
{

class LuaInterpreter
{
  public:
    LuaInterpreter();
    ~LuaInterpreter();

    bool runScriptFile(const std::string& scriptFile);
    bool runScriptString(const std::string& scriptString);

  private:
    void log(uint8_t level, const std::string& fmt, sol::variadic_args va);
    sol::state lua;
};

}  // namespace mod

#endif