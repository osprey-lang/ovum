#ifndef VM__VM_H
#define VM__VM_H

// Define some macros for the target OS.
// The following macros will be defined as 0 or 1 depending
// on what the target OS is:
//   OVUM_UNIX
//   OVUM_WINDOWS
//
// In the future, this list will be expanded. More than one
// macro at a time may be 1; when compiling against Debian,
// say, the macros OVUM_DEBIAN, OVUM_LINUX and OVUM_UNIX might
// all be true. That way, "#if OVUM_LINUX" will be usable as
// a way of targetting all Linux distros, while "#if OVUM_DEBIAN"
// would be used to narrow it down to Debian only.
// That is why there is not just a single OVUM_TARGET macro
// with a numeric value or somesuch.

#ifndef OVUM_HAS_TARGET_OS

// Let's try to figure out the OS.

#ifdef _WIN32
# define OVUM_WINDOWS 1
#else
# error Ovum is not yet supported on the target operating system.
#endif

// Assign 0 to macros that haven't been defined yet.

#ifndef OVUM_WINDOWS
# define OVUM_WINDOWS 0
#endif
#ifndef OVUM_UNIX
# define OVUM_UNIX 0
#endif

#define OVUM_HAS_TARGET_OS
#endif // OVUM_HAS_TARGET_OS

// Define an OVUM_WCHAR_SIZE macro for text functions

#ifndef OVUM_WCHAR_SIZE

#if OVUM_WINDOWS
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

#endif // OVUM_WCHAR_SIZE

#ifndef OVUM_API

#ifdef VM_EXPORTS
# define _OVUM_API __declspec(dllexport)
#else
# pragma comment(lib, "ovum-vm.lib")
# define _OVUM_API __declspec(dllimport)
#endif

#ifdef __cplusplus
#define OVUM_API  extern "C" _OVUM_API
#else
#define OVUM_API  _OVUM_API
#endif

#endif // OVUM_API

// Handle types. The OVUM_HANDLES_DEFINED macro lets these
// types be defined as something else by whatever includes
// this header file, usually for internal use. This should
// be used with caution.

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
#endif // OVUM_HANDLES_DEFINED

// Standard Ovum error codes.

#define OVUM_SUCCESS                0  /* EVERYTHING IS FINE. THERE IS NOTHING TO WORRY ABOUT. */
#define OVUM_ERROR_THROWN           1  /* An error was thrown using the VM_Throw function or Osprey's 'throw' keyword. */
#define OVUM_ERROR_UNSPECIFIED      2  /* An unspecified error occurred. */
#define OVUM_ERROR_METHOD_INIT      3  /* A method could not be initialized (e.g. due to an invalid opcode). */
#define OVUM_ERROR_NO_MEMORY        4  /* A memory allocation failed due to insufficient memory. */
#define OVUM_ERROR_NO_MAIN_METHOD   5  /* The startup module has no main method, or the main method is invalid. */
#define OVUM_ERROR_MODULE_LOAD      6  /* A module could not be loaded. */
#define OVUM_ERROR_OVERFLOW         8  /* Arithmetic overflow. */
#define OVUM_ERROR_DIVIDE_BY_ZERO   9  /* Integer division by zero. */
#define OVUM_ERROR_INTERRUPTED     10  /* The thread was interrupted while waiting for a blocking operation. */
#define OVUM_ERROR_WRONG_THREAD    11  /* Attempting an operation on the wrong thread, such as leaving a mutex that the caller isn't in. */
#define OVUM_ERROR_BUSY           (-1) /* A semaphore, mutex or similar value is entered by another thread. */


// Include some general useful API functions, as well as
// the required ov_pathchar.h.
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

OVUM_API int VM_GetArgCount(ThreadHandle thread);
OVUM_API int VM_GetArgs(ThreadHandle thread, const int destLength, String *dest[]);
OVUM_API int VM_GetArgValues(ThreadHandle thread, const int destLength, Value dest[]);

#endif // VM__VM_H