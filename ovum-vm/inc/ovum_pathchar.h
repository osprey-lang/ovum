#ifndef OVUM__PATHCHAR_H
#define OVUM__PATHCHAR_H

// This header file exports the following members:
//   pathchar_t:
//      Represents a character used in a path name.
//   OVUM_PATH:
//      A macro to turn a string literal into an appropriate path literal,
//      e.g. OVUM_PATH("abc.txt")
//   OVUM_PATH_SEP, OVUM_PATH_SEP_ALT:
//      String literals containing the primary and secondary path separators,
//      as determined by the OS, of a type suitable for pathchar_t.
//      E.g. on Windows, they expand to L"\\" and L"/".
//      These values may be identical.
//   OVUM_PATH_SEPC, OVUM_PATH_SEPC_ALT:
//      Character versions of the previous.
//   OVUM_PATHNF:
//      Format placeholder for pathchar_t* in printf, as a string literal.
//   OVUM_PATHNWF:
//      Format placeholder for pathchar_t* in wprintf, as a wide-char string literal.
//
// Note: OVUM_PATH_SEP[C][_ALT] and OVUM_PATHN[W]F are macros so that they can
// be used to concatenate string literals, if it should ever be necessary.

#include "ovum.h"

// First, select an appropriate size for pathchar_t, if one isn't
// defined already.
// If OVUM_WIDE_PATHCHAR is 1, then it'll be two bytes (UTF-16);
// otherwise, one-byte "characters" (UTF-8 or ASCII, usually).

#ifndef OVUM_WIDE_PATHCHAR
# if OVUM_WINDOWS
#  define OVUM_WIDE_PATHCHAR 1
# else
#  define OVUM_WIDE_PATHCHAR 0
# endif
#endif // OVUM_WIDE_PATHCHAR

// Define some basic macros for dealing with pathname literals.

#if OVUM_WIDE_PATHCHAR

typedef wchar_t pathchar_t;

#define OVUM_PATH_(x)     L ## x
#define OVUM_PATH(x)      OVUM_PATH_(x)

#define OVUM_PATHNF        "%ls"
#define OVUM_PATHNWF       L"%ls"

#else

typedef char pathchar_t;

#define OVUM_PATH(x)      x

#define OVUM_PATHNF        "%s"
#define OVUM_PATHNWF       L"%s"

#endif

// Finally, define the platform-specific path separator macros.

#if OVUM_WINDOWS

#define OVUM_PATH_SEP      OVUM_PATH("\\")
#define OVUM_PATH_SEP_ALT  OVUM_PATH("/")
#define OVUM_PATH_SEPC     OVUM_PATH('\\')
#define OVUM_PATH_SEPC_ALT OVUM_PATH('/')

#else

#define OVUM_PATH_SEP      OVUM_PATH("/")
#define OVUM_PATH_SEP_ALT  OVUM_PATH("\\")
#define OVUM_PATH_SEPC     OVUM_PATH('/')
#define OVUM_PATH_SEPC_ALT OVUM_PATH('\\')

#endif

#endif // OVUM__PATHCHAR_H
