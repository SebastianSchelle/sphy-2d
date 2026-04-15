#include "sphy-bindings.hpp"
#include <daScript/daScript.h>
#include <logging.hpp>
#include <string>

namespace
{
using namespace das;

void lg_d(const char* msg)
{
    LG_D("{}", msg ? msg : "");
}

void lg_i(const char* msg)
{
    LG_I("{}", msg ? msg : "");
}

void lg_w(const char* msg)
{
    LG_W("{}", msg ? msg : "");
}

void lg_e(const char* msg)
{
    LG_E("{}", msg ? msg : "");
}

class Module_SphyBindings : public Module
{
  public:
    Module_SphyBindings() : Module("sphy_bindings")
    {
        ModuleLibrary lib(this);
        lib.addBuiltInModule();

        addExtern<DAS_BIND_FUN(lg_d)>(
            *this, lib, "lg_d", SideEffects::modifyExternal, "lg_d")
            ->args({"msg"});
        addExtern<DAS_BIND_FUN(lg_i)>(
            *this, lib, "lg_i", SideEffects::modifyExternal, "lg_i")
            ->args({"msg"});
        addExtern<DAS_BIND_FUN(lg_w)>(
            *this, lib, "lg_w", SideEffects::modifyExternal, "lg_w")
            ->args({"msg"});
        addExtern<DAS_BIND_FUN(lg_e)>(
            *this, lib, "lg_e", SideEffects::modifyExternal, "lg_e")
            ->args({"msg"});

        verifyAotReady();
    }
};

REGISTER_MODULE(Module_SphyBindings);
}  // namespace

namespace mod
{

void touchSphyBindingsModule()
{
    // REGISTER_MODULE only declares the registrar function; we still need to
    // call it so the module instance exists before script compilation.
    static bool registered = false;
    if (registered)
    {
        return;
    }
    register_Module_SphyBindings();
    registered = true;
}

}  // namespace mod
