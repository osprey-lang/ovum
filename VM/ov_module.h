#pragma once

#ifndef VM__MODULE_H
#define VM__MODULE_H

#include "ov_vm.h"


// Obtains a handle to the module with the specified name.
// NOTE: the module must be loaded into memory! If it is not,
// this method will return NULL.
OVUM_API ModuleHandle FindModule(String *name);

// Searches a module for a type by the specified name.
// If the type could not be found, or if the type is private and includeInternal is false,
// this method returns NULL.
OVUM_API TypeHandle Module_FindType(ModuleHandle module, String *name, bool includeInternal);

// Searches a module for a global function by the specified name.
// If the function could not be found, or if the type is private and includeInternal is false,
// this method returns NULL.
OVUM_API MethodHandle Module_FindGlobalFunction(ModuleHandle module, String *name, bool includeInternal);

OVUM_API const bool Module_FindConstant(ModuleHandle module, String *name, bool includeInternal, Value &result);


#endif // VM__MODULE_H