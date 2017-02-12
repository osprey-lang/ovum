#ifndef OVUM__VM_H
#define OVUM__VM_H

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

# ifdef _WIN32
#  define OVUM_WINDOWS 1
# else
#  error Ovum is not yet supported on the target operating system.
# endif

# define OVUM_HAS_TARGET_OS
#endif // OVUM_HAS_TARGET_OS

// Assign 0 to OS macros that haven't been defined yet.

#ifndef OVUM_WINDOWS
# define OVUM_WINDOWS 0
#endif

#ifndef OVUM_UNIX
# define OVUM_UNIX 0
#endif

// OVUM_API macro for decorating API functions with.

#ifndef OVUM_API
# ifdef VM_EXPORTS
#  define OVUM_API_ __declspec(dllexport)
# else
#  pragma comment(lib, "ovum-vm.lib")
#  define OVUM_API_ __declspec(dllimport)
# endif

# ifdef __cplusplus
#  define OVUM_API  extern "C" OVUM_API_
# else
#  define OVUM_API  OVUM_API_
# endif
#endif // OVUM_API

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

// NOTE: If you add more handle types, verify their sizes in ovum_compat.h.
// See that file for details.

#define OVUM_HANDLES_DEFINED
#endif // OVUM_HANDLES_DEFINED

#include "ovum_compat.h"

// Forward declarations and typedefs of common/fundamental types

// All Ovum strings are UTF-16, guaranteed.
#if OVUM_WCHAR_SIZE == 2
// If sizeof(wchar_t) is 2, we assume it's UTF-16, and
// define ovchar_t to be of that type.
typedef wchar_t ovchar_t;
#else
// Otherwise, we have to fall back to uint16_t.
typedef uint16_t ovchar_t;
#endif

// The ovlocals_t type is used for all kinds of "local" counts. That is,
// counts of "local" values - parameters, arguments, local variables,
// stack slots, etc. Its use is fairly broad, but it should particularly
// be used for argument counts.
typedef uint32_t ovlocals_t;

#define OVLOCALS_MAX UINT32_MAX

typedef struct Value_S Value;
typedef struct String_S String;
typedef struct ListInst_S ListInst;
typedef struct ErrorInst_S ErrorInst;
typedef struct MethodInst_S MethodInst;

// And now let's actually define all of those lovely types.

// The Value struct is the primary means of representing a value as
// seen by the VM. A Value consists of a type handle (which may be
// null to represent the null reference), and up to eight bytes of
// instance data.
// The instance data is typically a pointer to some memory, typical-
// ly containing several adjacent Values with the field values of
// the instance. If the type is primitive (IS_PRIMITIVE(value) is
// true), then the eight bytes of instance data directly contain the
// value of the instance. Usually the 'integer', 'uinteger' or 'real'
// field is used for this purpose. Finally, a Value may represent a
// reference (IS_REFERENCE(value) is true), in which case the 'reference'
// field points to the referent's storage location.
// The value of a Value (as it were) should usually only be touched
// directly by the methods in the Value's type, and by the VM. Many
// types with native implementations store custom C structs or C++
// classes behind the instance pointer. Do not rely on it pointing
// to an array of Value fields.
struct Value_S
{
	TypeHandle type;

	union
	{
		// Primitive values get eight (8) bytes to play with.
		// The 'integer', 'uinteger' or 'real' field is usually
		// used instead.
		unsigned char raw[8];
		int64_t integer;
		uint64_t uinteger;
		double real;

		// The instance is just a pointer to some bytes.
		uint8_t *instance;

		// Let's make some common, fixed-layout types easily available.
		String *string;
		ListInst *list;
		ErrorInst *error;
		MethodInst *method;

		// References make use of this field. It does NOT always point
		// to a Value! Use the ReadReference and WriteReference functions
		// to access references.
		void *reference;
	} v;

#ifdef __cplusplus
	// Gets the instance data as a specific type. This method does not
	// verify that the instance data actually is of the specified type;
	// it merely casts the instance pointer.
	template<class T>
	inline T *Get()
	{
		return reinterpret_cast<T*>(v.instance);
	}

	// Gets the instance data as a specific type, from a given offset
	// relative to the instance data pointer. This method does not verify
	// that the instance data actually is of the specified type; it merely
	// casts the instance pointer.
	template<class T>
	inline T *Get(uint32_t offset)
	{
		return reinterpret_cast<T*>(v.instance + offset);
	}
#endif
};

enum class StringFlags : uint32_t
{
	NONE = 0,
	// Tells the GC not to collect a String*, because it was created
	// from some static resource.
	STATIC = 1,
	// The string has been hashed (its hashCode contains a usable value).
	// This should ONLY be set by String_GetHashCode! If you set this flag
	// anywhere else, you will break the VM. Probably. It's C++. Who knows.
	HASHED = 2,
	// The string is interned. This flag is only used by the GC, to determine
	// whether the string needs to be removed from the intern table when it
	// is collected.
	INTERN = 4,
};
OVUM_ENUM_OPS(StringFlags, uint32_t);

// Note: Strings are variable-size instances, and should never be passed by value.
// Always pass String pointers around. To get the character data, take the address
// of the firstChar field.
struct String_S
{
	// The length of the string, not including the terminating \0.
	const size_t length;
	// The string's hash code. If the string has had its hash code calculated (if the
	// flag StringFlags::HASHED is set), then this field contains the hash code of the
	// string. Otherwise, this value is meaningless.
	int32_t hashCode;
	// If the flags contain StringFlags::STATIC, the String is never garbage collected,
	// as it comes from a static resource.
	// If the flags contain StringFlags::HASHED, then hashCode contains the string's
	// hash code. Otherwise, don't rely on it.
	StringFlags flags;
	// The first character. The rest of the string is laid out directly afert this field.
	const ovchar_t firstChar;
};

struct ListInst_S
{
	size_t capacity; // the length of 'values'
	size_t length;   // the actual number of items contained in the list
	int32_t version;  // the "version" of the list, which is incremented each time the list changes
	Value *values;    // the values contained in the list
};

struct ErrorInst_S
{
	String *message;
	String *stackTrace;
	Value innerError;
	Value data;
};

struct MethodInst_S
{
	Value instance;
	MethodHandle method;
};

// Include some general useful API functions, as well as
// the required ov_pathchar.h.
#include "ovum_value.h"
#include "ovum_type.h"
#include "ovum_thread.h"
#include "ovum_gc.h"
#include "ovum_helpers.h"
#include "ovum_pathchar.h"

OVUM_API void VM_Print(String *str);
OVUM_API void VM_PrintLn(String *str);

OVUM_API void VM_PrintErr(String *str);
OVUM_API void VM_PrintErrLn(String *str);

OVUM_API size_t VM_GetArgCount(ThreadHandle thread);
OVUM_API size_t VM_GetArgs(ThreadHandle thread, size_t destLength, String *dest[]);
OVUM_API size_t VM_GetArgValues(ThreadHandle thread, size_t destLength, Value dest[]);

#endif // OVUM__VM_H
