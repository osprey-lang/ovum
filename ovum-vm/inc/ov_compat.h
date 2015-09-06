#ifndef VM__COMPAT_H
#define VM__COMPAT_H

/*
 * This file contains various compatibility and utility macros.
 */

// All parts of Ovum require standard integer types
#include <stdint.h>

// OVUM_64BIT is true if Ovum is compiled as a 64-bit application

#ifndef OVUM_64BIT

# if defined(_MSC_VER) || defined(_WIN32)
#  ifdef _WIN64
#   define OVUM_64BIT 1
#  else
#   define OVUM_64BIT 0
#  endif

# elif defined(__GNUC__)
#  if __x86_64__ || __ppc64__
#   define OVUM_64BIT 1
#  else
#   define OVUM_64BIT
#  endif

# else
#  error Unable to determine whether Ovum is 32- or 64-bit
# endif

#endif // OVUM_64BIT

// The OVUM_WCHAR_SIZE macro is used in various text functions

#ifndef OVUM_WCHAR_SIZE
# if OVUM_WINDOWS
// wchar_t is UTF-16 on Windows
#  define OVUM_WCHAR_SIZE 2

# elif defined(__SIZEOF_WCHAR_T__)
#  define OVUM_WCHAR_SIZE __SIZEOF_WCHAR_T__

# elif defined(__WCHAR_MAX__)
#  if __WCHAR_MAX__ > 0xFFFF
#   define OVUM_WCHAR_SIZE 4
#  else
#   define OVUM_WCHAR_SIZE 2
#  endif

# else
#  include <wchar.h>
#  ifndef WCHAR_MAX
#   error Problem with wchar.h: doesn't define WCHAR_MAX!
#  endif
#  if WCHAR_MAX > 0xFFFF
#   define OVUM_WCHAR_SIZE 4
#  else
#   define OVUM_WCHAR_SIZE 2
#  endif
# endif
#endif // OVUM_WCHAR_SIZE

// Defines "proper" bitwise operators for the enum type TEnum, with the underlying
// integer type TInt. The operators only work with TEnum values, and always return
// TEnum values.

#define OVUM_ENUM_OPS(TEnum,TInt) \
	inline TEnum operator&(TEnum a, TEnum b) { return static_cast<TEnum>((TInt)a & (TInt)b); } \
	inline TEnum operator|(TEnum a, TEnum b) { return static_cast<TEnum>((TInt)a | (TInt)b); } \
	inline TEnum operator^(TEnum a, TEnum b) { return static_cast<TEnum>((TInt)a ^ (TInt)b); } \
	inline TEnum &operator&=(TEnum &a, TEnum b) { a = a & b; return a; } \
	inline TEnum &operator|=(TEnum &a, TEnum b) { a = a | b; return a; } \
	inline TEnum &operator^=(TEnum &a, TEnum b) { a = a ^ b; return a; } \
	inline TEnum operator~(TEnum a) { return static_cast<TEnum>(~(TInt)a); }

// For checked multiplication only
// (uses the _mul128 function if available)
#ifndef OVUM_USE_INTRINSICS
# if !defined(__GNUC__) && defined(_M_AMD64)
#  include <intrin.h>
#  define OVUM_USE_INTRINSICS 1
# else
#  define OVUM_USE_INTRINSICS 0
# endif
#endif // OVUM_USE_INTRINSICS

#ifndef OVUM_ALIGN_TO
// This macro is effectively equivalent to ceil(size / alignment) * alignment,
// but for integer types, and if size and alignment are both constant values, it
// can be fully evaluated at compile-time.
# define OVUM_ALIGN_TO(size,alignment)  (((size) + (alignment) - 1) / (alignment) * (alignment))
#endif // OVUM_ALIGN_TO

#ifndef OVUM_DELETE
// Marks a function member as having been deleted, if the compiler supports it.
// TODO: Better conditional definition based on compiler capabilities.
# if defined(_MSC_VER) && _MSC_VER > 1800
#  define OVUM_DELETE =delete
# else
#  define OVUM_DELETE
# endif
#endif // OVUM_DELETE

#ifndef OVUM_DISBALE_COPY_AND_ASSIGN
// When put in the 'private' section of a type definition, disables copying and
// assignment, by declaring operator= and the copy constructor as private members,
// and deleting them if the compiler supports it. This should be used on any type
// that must not be copied by value.
# define OVUM_DISABLE_COPY_AND_ASSIGN(TypeName)  \
	TypeName(const TypeName&) OVUM_DELETE; \
	void operator=(const TypeName&) OVUM_DELETE
#endif // OVUM_DISBALE_COPY_AND_ASSIGN

#ifndef OVUM_DISABLE_IMPLICIT_CONSTRUCTION
// Disallows parameterless construction, in addition to disabling the copy constructor
// and operator=. This must be put in the 'private' section of a type definition.
# define OVUM_DISABLE_IMPLICIT_CONSTRUCTION(TypeName) \
	TypeName() OVUM_DELETE;                     \
	OVUM_DISABLE_COPY_AND_ASSIGN(TypeName)
#endif // OVUM_DISABLE_IMPLICIT_CONSTRUCTION

#ifndef OVUM_CDECL
# if defined(_MSC_VER)
#  define OVUM_CDECL __cdecl
# elif defined(__GNUC__)
#  define OVUM_CDECL __attribute__((cdecl))
# else
// Disable the feature
#  define OVUM_CDECL
# endif
#endif // OVUM_CDECL

#ifndef OVUM_NOINLINE
# if defined(_MSC_VER)
#  define OVUM_NOINLINE __declspec(noinline)
# elif defined(__GNUC__)
#  define OVUM_NOINLINE __attribute__((noinline))
# else
// Disable the feature
#  define OVUM_NOINLINE
# endif
#endif // OVUM_NOINLINE

#ifndef OVUM_DEBUG
# if defined(DEBUG)
#  define OVUM_DEBUG 1
# elif defined(_DEBUG) // MSVC
#  define OVUM_DEBUG 1
# else
#  define OVUM_DEBUG 0
# endif
#endif // OVUM_DEBUG

#ifndef OVUM_ASSERT
# if OVUM_DEBUG
// Only include this source file in debug mode! Otherwise OVUM_DEBUG is empty.
#  include <assert.h>
#  define OVUM_ASSERT_(expr) assert(expr)
#  define OVUM_ASSERT(expr) OVUM_ASSERT_(expr)
# else
#  define OVUM_ASSERT(expr) ((void)0)
# endif
#endif // OVUM_ASSERT

#endif // VM__COMPAT_H
