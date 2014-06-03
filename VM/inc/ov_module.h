#pragma once

#ifndef VM__MODULE_H
#define VM__MODULE_H

#include "ov_vm.h"

typedef struct ModuleVersion_S
{
	int32_t major;
	int32_t minor;
	int32_t build;
	int32_t revision;
} ModuleVersion;

inline bool operator==(const ModuleVersion &a, const ModuleVersion &b)
{
	return a.major == b.major &&
		a.minor == b.minor &&
		a.build == b.build &&
		a.revision == b.revision;
}
inline bool operator!=(const ModuleVersion &a, const ModuleVersion &b)
{
	return !(a == b);
}

enum class ModuleMemberFlags : uint32_t
{
	// Mask for extracting the kind of member (type, function or constant).
	KIND     = 0x000f,

	NONE     = 0x0000,

	TYPE     = 0x0001,
	FUNCTION = 0x0002,
	CONSTANT = 0x0003,

	PROTECTION = 0x00f0,
	PUBLIC     = 0x0010,
	INTERNAL   = 0x0020,
};
ENUM_OPS(ModuleMemberFlags, uint32_t);

typedef struct GlobalMember_S
{
	ModuleMemberFlags flags;
	String *name;
	union
	{
		TypeHandle type;
		MethodHandle function;
		Value constant;
	};
} GlobalMember;

// Obtains a handle to the module with the specified name and version.
//
// Parameters:
//   name:
//     The name of the module to find.
//   version:
//     The desired version of the module. If null, this function will
//     return the first encountered module with the given name.
OVUM_API ModuleHandle FindModule(String *name, ModuleVersion *version);

// Gets the name of the specified module.
OVUM_API String *Module_GetName(ModuleHandle module);

// Gets the version number of the specified module.
OVUM_API void Module_GetVersion(ModuleHandle module, ModuleVersion *version);

// Gets the name of the file from which the module was loaded.
OVUM_API String *Module_GetFileName(ThreadHandle thread, ModuleHandle module);

// Searches a module for a global member with the specified name.
// If the member could not be found, or if the member is private and includeInternal is false,
// this method returns false and 'result' is not assigned to. Otherwise, 'result' contains the
// member and the method returns true.
OVUM_API bool Module_GetGlobalMember(ModuleHandle module, String *name, bool includeInternal, GlobalMember *result);

// Searches a module for a type with the specified name.
// If the type could not be found, or if the type is private and includeInternal is false,
// this method returns null.
OVUM_API TypeHandle Module_FindType(ModuleHandle module, String *name, bool includeInternal);

// Searches a module for a global function with the specified name.
// If the function could not be found, or if the type is private and includeInternal is false,
// this method returns null.
OVUM_API MethodHandle Module_FindGlobalFunction(ModuleHandle module, String *name, bool includeInternal);

// Searches a module for a global constant with the specified name.
// If the constant could not be found, or if the constant is private and includeInternal is false,
// this method returns false and does not write to 'result'. Otherwise, 'result' contains the value
// of the constant, and the method returns true.
OVUM_API bool Module_FindConstant(ModuleHandle module, String *name, bool includeInternal, Value &result);

// Locates the entry point with the specified name in the native library
// of the given module. If the module has no native library or the entry
// point doesn't exist, this method returns null.
OVUM_API void *Module_FindNativeFunction(ModuleHandle module, const char *name);


#endif // VM__MODULE_H