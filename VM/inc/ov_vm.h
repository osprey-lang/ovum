#pragma once

#ifndef VM__VM_H
#define VM__VM_H

#define OVUM_UNIX    0
#define OVUM_WINDOWS 1

#ifndef OVUM_TARGET
#ifdef _WIN32
# define OVUM_TARGET OVUM_WINDOWS
#else
# define OVUM_TARGET OVUM_UNIX
# error Ovum does not support the target operating system.
#endif
#endif

#ifndef OVUM_WCHAR_SIZE
// Define an OVUM_WCHAR_SIZE macro for text functions
#if OVUM_TARGET == OVUM_WINDOWS
// wchar_t is UTF-16 on Windows
# define OVUM_WCHAR_SIZE 2
#elif defined(__SIZEOF_WCHAR_T__)
# define OVUM_WCHAR_SIZE __SIZEOF_WCHAR_T__
#elif defined(__WCHAR_MAX__)
# if __WCHAR_MAX__ > 0xFFFF
#  define OVUM_WCHAR_SIZE 4
# else
#  define OVUM_WCHAR_SIZE 2
# endif
#else
# include <wchar.h>
# ifndef WCHAR_MAX
#  error Problem with wchar.h: doesn't define WCHAR_MAX!
# endif
# if WCHAR_MAX > 0xFFFF
#  define OVUM_WCHAR_SIZE 4
# else
#  define OVUM_WCHAR_SIZE 2
# endif
#endif
#endif

#ifdef VM_EXPORTS
# define _OVUM_API __declspec(dllexport)
#else
# pragma comment(lib, "VM.lib")
# define _OVUM_API __declspec(dllimport)
#endif

#define OVUM_API	extern "C" _OVUM_API

#ifndef OVUM_HANDLES_DEFINED
// Represents a handle to a specific thread.
typedef void *const ThreadHandle;
// Represents a handle to a specific type.
typedef void *TypeHandle;
// Represents a handle to a specific module.
typedef void *ModuleHandle;
// Represents a handle to a member of a type.
typedef void *MemberHandle;
// Represents a handle to a method, with one or more overloads.
typedef void *MethodHandle;
// Represents a handle to a single method overload.
typedef void *OverloadHandle;
// Represents a handle to a field.
typedef void *FieldHandle;
// Represents a handle to a property.
typedef void *PropertyHandle;

#define OVUM_HANDLES_DEFINED
#endif

// Some header files!
#include "ov_value.h"
#include "ov_thread.h"
#include "ov_gc.h"
#include "ov_module.h"
#include "ov_helpers.h"
#include "ov_pathchar.h"

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
	const pathchar_t *startupFile;
	// The path to the directory containing the module library.
	// For details on how module names are resolved, see the
	// comments in ov_module.internal.h/Module::OpenByName.
	// This will be moved to proper documentation eventually.
	const pathchar_t *modulePath;
	// Make the VM be more explicit about what it's doing during startup.
	bool verbose;
} VMStartParams;

OVUM_API int VM_Start(VMStartParams *params);

OVUM_API void VM_Print(String *str);
OVUM_API void VM_PrintLn(String *str);

OVUM_API void VM_PrintErr(String *str);
OVUM_API void VM_PrintErrLn(String *str);

OVUM_API int VM_GetArgCount();
OVUM_API int VM_GetArgs(const int destLength, String *dest[]);
OVUM_API int VM_GetArgValues(const int destLength, Value dest[]);

#endif // VM__VM_H