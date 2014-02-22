#pragma once

#ifndef VM__VM_H
#define VM__VM_H

#ifndef VM_EXPORTS
#pragma comment(lib, "VM.lib")
#endif

#ifdef VM_EXPORTS
#define _OVUM_API __declspec(dllexport)
#else
#define _OVUM_API __declspec(dllimport)
#endif

#define OVUM_API	extern "C" _OVUM_API

#ifndef UNICODE
#error You are not supposed to compile Ovum without Unicode support.
#endif

#ifndef VM_EXPORTS
// Represents a handle to a specific thread.
typedef void *const ThreadHandle;
// Represents a handle to a specific type.
typedef void *TypeHandle;
// Represents a handle to a specific module.
typedef void *ModuleHandle;
// Represents a handle to a member of a type.
typedef void *MemberHandle;
// Represents a handle to a method.
typedef void *MethodHandle;
// Represents a handle to a field.
typedef void *FieldHandle;
// Represents a handle to a property.
typedef void *PropertyHandle;
// (These are defined here because they're used in a lot of places.)
#endif

// Some header files!
#include "ov_value.h"
#include "ov_thread.h"
#include "ov_gc.h"
#include "ov_module.h"
#include "ov_helpers.h"

typedef struct VMStartParams_S
{
	// The number of arguments passed to the program.
	int argc;
	// The actual arguments passed to the program.
	const wchar_t **argv;
	// The file from which to load the program to be executed.
	// This must be a full path, because of peculiarities in
	// the way Windows deals with current working directories.
	// If it is a relative path, expect strange behaviour.
	const wchar_t *startupFile;
	// And this is the path to the directory that contains the
	// module library. Modules are located by first looking in
	// the startup file's directory, and if it can't be found
	// there, the VM examines this modulePath.
	// NO OTHER DIRECTORIES ARE CONSIDERED. The VM does NOT care
	// about PATH or any other environment variables. At all.
	const wchar_t *modulePath;
	// Make the VM be more explicit about what it's doing during startup.
	bool verbose;
} VMStartParams;

OVUM_API int VM_Start(VMStartParams params);

OVUM_API void VM_Print(String *str);
OVUM_API void VM_PrintLn(String *str);

OVUM_API void VM_PrintErr(String *str);
OVUM_API void VM_PrintErrLn(String *str);

OVUM_API int VM_GetArgCount();
OVUM_API int VM_GetArgs(const int destLength, String *dest[]);
OVUM_API int VM_GetArgValues(const int destLength, Value dest[]);

#endif // VM__VM_H