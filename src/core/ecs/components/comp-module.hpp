#ifndef COMP_MODULE_HPP
#define COMP_MODULE_HPP

#include "comp-ident.hpp"
#include "std-inc.hpp"

namespace ecs
{
struct Module
{
    GenericHandle moduleHandle;
};

#define SER_MODULE SOBJ(o.moduleHandle);
EXT_SER(Module, SER_MODULE)
EXT_DES(Module, SER_MODULE)

}  // namespace ecs
#endif