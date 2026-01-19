#include "lua-interpreter.hpp"
#include "spdlog/fmt/bundled/format.h"
#include <filesystem>
#include <logging.hpp>

namespace mod
{

LuaInterpreter::LuaInterpreter()
{
    lua.open_libraries(sol::lib::base, sol::lib::package);
    auto dbg = lua.create_named_table("dbg");
    dbg.set_function("logI",
                     [this](std::string fmt, sol::variadic_args va) { log(1, "Lua: " + fmt, va); });
    dbg.set_function("logD",
                     [this](std::string fmt, sol::variadic_args va) { log(0, "Lua: " + fmt, va); });
    dbg.set_function("logW",
                     [this](std::string fmt, sol::variadic_args va) { log(2, "Lua: " + fmt, va); });
    dbg.set_function("logE",
                     [this](std::string fmt, sol::variadic_args va) { log(3, "Lua: " + fmt, va); });
}

LuaInterpreter::~LuaInterpreter() {}

void LuaInterpreter::log(uint8_t level,
                         const std::string& fmt,
                         sol::variadic_args va)
{
    try
    {
        std::string formatted;
        
        size_t placeholder_count = 0;
        for (size_t i = 0; i < fmt.length(); ++i)
        {
            if (fmt[i] == '{')
            {
                if (i + 1 < fmt.length() && fmt[i + 1] == '{')
                {
                    i++;
                    continue;
                }
                else if (i + 1 < fmt.length() && fmt[i + 1] == '}')
                {
                    placeholder_count++;
                    i++;
                }
                else
                {
                    size_t end = fmt.find('}', i);
                    if (end != std::string::npos)
                    {
                        placeholder_count++;
                        i = end;
                    }
                }
            }
        }
        
        if (placeholder_count <= va.size())
        {
            formatted = fmt;
            size_t arg_idx = 0;
            size_t pos = 0;
            
            while (pos < formatted.length() && arg_idx < va.size())
            {
                size_t open_brace = formatted.find('{', pos);
                if (open_brace == std::string::npos)
                    break;
                
                // Check for escaped brace {{
                if (open_brace + 1 < formatted.length() && formatted[open_brace + 1] == '{')
                {
                    pos = open_brace + 2;
                    continue;
                }
                    
                size_t close_brace = formatted.find('}', open_brace);
                if (close_brace == std::string::npos)
                    break;
                
                std::string format_spec = formatted.substr(open_brace + 1, close_brace - open_brace - 1);
                
                std::string arg_str;
                auto v = va[arg_idx];
                sol::type t = v.get_type();
                
                if (t == sol::type::number)
                {
                    sol::optional<int> int_val = v.as<sol::optional<int>>();
                    if (int_val && format_spec.empty())
                    {
                        arg_str = std::to_string(*int_val);
                    }
                    else
                    {
                        double dbl_val = v.as<double>();
                        if (format_spec.empty())
                        {
                            arg_str = std::to_string(dbl_val);
                        }
                        else
                        {
                            std::string spec_fmt = "{" + format_spec + "}";
                            fmt::memory_buffer buf;
                            fmt::format_to(std::back_inserter(buf), fmt::runtime(spec_fmt), dbl_val);
                            arg_str = std::string(buf.data(), buf.size());
                        }
                    }
                }
                else if (t == sol::type::string)
                {
                    std::string str_val = v.as<std::string>();
                    if (format_spec.empty())
                    {
                        arg_str = str_val;
                    }
                    else
                    {
                        std::string spec_fmt = "{" + format_spec + "}";
                        fmt::memory_buffer buf;
                        fmt::format_to(std::back_inserter(buf), fmt::runtime(spec_fmt), str_val);
                        arg_str = std::string(buf.data(), buf.size());
                    }
                }
                else if (t == sol::type::boolean)
                {
                    bool bool_val = v.as<bool>();
                    arg_str = bool_val ? "true" : "false";
                }
                else
                {
                    std::string str_repr = v.as<std::string>();
                    arg_str = str_repr;
                }
                
                formatted.replace(open_brace, close_brace - open_brace + 1, arg_str);
                pos = open_brace + arg_str.length();
                arg_idx++;
            }
            
            size_t escaped_pos = 0;
            while ((escaped_pos = formatted.find("{{", escaped_pos)) != std::string::npos)
            {
                formatted.replace(escaped_pos, 2, "{");
                escaped_pos += 1;
            }
            escaped_pos = 0;
            while ((escaped_pos = formatted.find("}}", escaped_pos)) != std::string::npos)
            {
                formatted.replace(escaped_pos, 2, "}");
                escaped_pos += 1;
            }
        }
        else
        {
            formatted = fmt + " [not enough arguments]";
        }
        
        switch (level)
        {
            case 0:
                LG_D("{}", formatted);
                break;
            case 1:
                LG_I("{}", formatted);
                break;
            case 2:
                LG_W("{}", formatted);
                break;
            case 3:
                LG_E("{}", formatted);
                break;
            default:
                LG_I("{}", formatted);
                break;
        }
    }
    catch (const std::exception& e)
    {
        LG_E("Error formatting log message: {}", e.what());
    }
}

bool LuaInterpreter::runScriptFile(const std::string& scriptFile)
{
    if (!std::filesystem::exists(scriptFile))
    {
        LG_E("Script file not found: {}", scriptFile);
        return false;
    }
    auto result = lua.script_file(scriptFile);
    if (!result.valid())
    {
        LG_E("Failed to run script file: {}", scriptFile);
        return false;
    }
    return true;
}

bool LuaInterpreter::runScriptString(const std::string& scriptString)
{
    auto result = lua.script(scriptString);
    if (!result.valid())
    {
        LG_E("Failed to run script string: {}", scriptString);
        return false;
    }
    return true;
}

}  // namespace mod