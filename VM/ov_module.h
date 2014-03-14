#pragma once

#ifndef VM__MODULE_H
#define VM__MODULE_H

#include "ov_vm.h"


// Obtains a handle to the module with the specified name.
// NOTE: the module must be loaded into memory! If it is not,
// this method will return null.
OVUM_API ModuleHandle FindModule(String *name);

// Searches a module for a type by the specified name.
// If the type could not be found, or if the type is private and includeInternal is false,
// this method returns null.
OVUM_API TypeHandle Module_FindType(ModuleHandle module, String *name, bool includeInternal);

// Searches a module for a global function by the specified name.
// If the function could not be found, or if the type is private and includeInternal is false,
// this method returns null.
OVUM_API MethodHandle Module_FindGlobalFunction(ModuleHandle module, String *name, bool includeInternal);

OVUM_API const bool Module_FindConstant(ModuleHandle module, String *name, bool includeInternal, Value &result);

// Locates the entry point with the specified name in the native library
// of the given module. If the module has no native library or the entry
// point doesn't exist, this method returns null.
OVUM_API void *Module_FindNativeFunction(ModuleHandle module, const char *name);


#endif // VM__MODULE_H