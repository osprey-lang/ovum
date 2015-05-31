#ifndef VM__PATHCHAR_H
#define VM__PATHCHAR_H

// This header file exports the following members:
//   pathchar_t:
//      Represents a character used in a path name.
//   _Path:
//      A macro to turn a string literal into an appropriate path literal,
//      e.g. _Path("abc.txt")
//   PATH_SEP, PATH_SEP_ALT:
//      String literals containing the primary and secondary path separators,
//      as determined by the OS, of a type suitable for pathchar_t and PathName.
//      E.g. on Windows, they expand to L"\\" and L"/".
//      These values may be identical.
//   PATH_SEPC, PATH_SEPC_ALT:
//      Character versions of the previous.
//   PATHNF:
//      Format placeholder for pathchar_t* in printf, as a string literal.
//   PATHNWF:
//      Format placeholder for pathchar_t* in wprintf, as a wide-char string literal.
//
// Note: PATH_SEP[C][_ALT] and PATHN[W]F are macros so that they can be used
// to concatenate string literals, if it should ever be necessary.

#include "ov_vm.h"

#if OVUM_WINDOWS
// pathchar_t is a wide character type, containing UTF-16.
#define OVUM_WIDE_PATHCHAR 1
#else
// pathchar_t is the regular char type, containing UTF-8.
#define OVUM_WIDE_PATHCHAR 0
#endif

#if OVUM_WIDE_PATHCHAR

typedef wchar_t pathchar_t;

#define __Path(x)     L ## x
#define _Path(x)      __Path(x)

#define PATH_SEP      _Path("\\")
#define PATH_SEP_ALT  _Path("/")
#define PATH_SEPC     _Path('\\')
#define PATH_SEPC_ALT _Path('/')

#define PATHNF        "%ls"
#define PATHNWF       L"%ls"

#else

typedef char pathchar_t;

#define _Path(x)      x

#define PATH_SEP      _Path("/")
#define PATH_SEP_ALT  _Path("\\")
#define PATH_SEPC     _Path('/')
#define PATH_SEPC_ALT _Path('\\')

#define PATHNF        "%s"
#define PATHNWF       L"%s"

#endif

#endif // VM__PATHCHAR_H