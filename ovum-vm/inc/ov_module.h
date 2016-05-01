#ifndef OVUM__MODULE_H
#define OVUM__MODULE_H

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
	CONSTANT = 0x0004,

	PROTECTION = 0x00f0,
	PUBLIC     = 0x0010,
	INTERNAL   = 0x0020,
};
OVUM_ENUM_OPS(ModuleMemberFlags, uint32_t);

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

// A StaticStateDeallocator, as the name suggests, is responsible for deallocating
// a native module's static state. The deallocator should free any memory used by
// the static state, as well as close any open file handles and other resources.
//
// These functions should be fail-safe, as there is no way to return a status code
// from a StaticStateDeallocator. This is by design: a StaticStateDeallocator is
// only called when the module is about to be unloaded, which occurs when the VM
// is shutting down. There is no opportunity for error handling at this point.
//
// Parameters:
//   state:
//     The module's static state. This will always be the same pointer that was
//     passed to Module_InitStaticState.
typedef void (*StaticStateDeallocator)(void *state);

// Obtains a handle to the module with the specified name and version.
//
// Parameters:
//   thread:
//     The current thread.
//   name:
//     The name of the module to find.
//   version:
//     The desired version of the module. If null, this function will
//     return the first encountered module with the given name.
OVUM_API ModuleHandle FindModule(ThreadHandle thread, String *name, ModuleVersion *version);

// Gets the name of the specified module.
OVUM_API String *Module_GetName(ModuleHandle module);

// Gets the version number of the specified module.
OVUM_API void Module_GetVersion(ModuleHandle module, ModuleVersion *version);

// Gets the name of the file from which the module was loaded.
OVUM_API String *Module_GetFileName(ThreadHandle thread, ModuleHandle module);

// Gets a pointer to the static state of the specified module, if the module has such a state.
// The static state is initialized by calling Module_InitStaticState; see that function for
// more details.
//
// Parameters:
//   module:
//     The module whose static state is to be looked up.
// Returns:
//   The module's static state, or null if it has none.
OVUM_API void *Module_GetStaticState(ModuleHandle module);

// Behaves exactly like Module_GetStaticState, except that it loads static state for the module
// of the managed call that is currently on top of the call stack.
//
// Parameters:
//   thread:
//     The current thread.
// Returns:
//   The module's static state, or null if it has none.
OVUM_API void *Module_GetCurrentStaticState(ThreadHandle thread);

// Initializes the module's static state. The static state is shared across the entire instance
// of the module, like a 'static' variable in Osprey. Unlike 'static' in C/C++, this function
// does NOT share its state with other instances of Ovum running in the same process. Using this
// function instead of C/C++ 'static' enables modules to be instantiated many times per process
// without risk of cross-contamination.
//
// This function should only be used by modules with native code, and only when necessary. Code
// in a managed language, like Osprey, should use that language's 'static' feature.
//
// Parameters:
//   module:
//     The module to initialize static state for. This function can only be called from the
//     module's OvumModuleMain method, and only during initialization. The call will be ignored
//     at all other times.
//   state:
//     The state to assign to the module. This pointer can be null.
//   deallocator:
//     A function that is called when the module is about to be unloaded. It is passed the value
///    of the 'state' parameter, and must deallocate the memory used by it. If this function is
//     null, the state is not applied to the module.
OVUM_API void Module_InitStaticState(ModuleHandle module, void *state, StaticStateDeallocator deallocator);

// Searches a module for a global member with the specified name.
// If the member could not be found, or if the member is private and includeInternal is false,
// this method returns false and 'result' is not assigned to. Otherwise, 'result' contains the
// member and the method returns true.
OVUM_API bool Module_GetGlobalMember(ModuleHandle module, String *name, bool includeInternal, GlobalMember *result);

// Gets the total number of members in the module.
OVUM_API int32_t Module_GetGlobalMemberCount(ModuleHandle module);

// Gets the global member at the specified index. The index must be between zero (inclusive)
// and the global member count (exclusive). Module_GetGlobalMemberCount returns the latter.
//
// If the index is valid, 'result' is updated with the result, and the function returns true.
// Otherwise, the function returns false and 'result' is not written to.
OVUM_API bool Module_GetGlobalMemberByIndex(ModuleHandle module, int32_t index, GlobalMember *result);

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

// Searches the specified module's imported modules (that is, other modules that
// the specified module depends on) for a module with the specified name.
//
// If the module does not import a module by the specified name, null is returned.
//
// Parameters:
//   module:
//     The module whose imported modules is to be searched.
//   name:
//     The name of the module to find.
OVUM_API ModuleHandle Module_FindDependency(ModuleHandle module, String *name);

// Gets the directories that are searched when resolving module dependencies.
// The VM tries several possible paths within each directory.
//
// Parameters:
//   thread:
//     The current thread.
//   resultSize:
//     The total size of the result buffer. No more than this number of values
//     will be written into the buffer.
//   count:
//     Receives the total number of directories. If the value is greater than
//     the buffer, it must be resized before all directories can be obtained.
// Returns:
//   An Ovum status code. This function can fail with an out-of-memory error.
OVUM_API int Module_GetSearchDirectories(ThreadHandle thread, int resultSize, String **result, int *count);

class ModuleMemberIterator
{
private:
	ModuleHandle module;
	int32_t index;
	bool updateCurrent;
	GlobalMember current;

public:
	inline ModuleMemberIterator(ModuleHandle module)
		: module(module), index(-1), updateCurrent(false)
	{
		current.flags = ModuleMemberFlags::NONE;
	}

	inline bool MoveNext()
	{
		if (index < Module_GetGlobalMemberCount(module) - 1)
		{
			index++;
			updateCurrent = true;
			return true;
		}

		return false;
	}

	inline GlobalMember &Current()
	{
		if (updateCurrent)
		{
			updateCurrent = false;
			Module_GetGlobalMemberByIndex(module, index, &current);
		}
		return current;
	}
};

#endif // OVUM__MODULE_H
