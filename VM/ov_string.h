#pragma once

#ifndef VM__STRING_H
#define VM__STRING_H

#include "ov_vm.h"

OVUM_API int32_t String_GetHashCode(String *str);
OVUM_API bool String_Equals(const String *a, const String *b);
OVUM_API bool String_EqualsIgnoreCase(const String *a, const String *b);

OVUM_API int String_Compare(const String *a, const String *b);

inline bool String_StartsWith(const String *a, const uchar ch)
{
	return a->firstChar == ch;
}
inline bool String_EndsWith(const String *a, const uchar ch)
{
	return (&a->firstChar)[a->length - 1] == ch;
}

OVUM_API String *String_ToUpper(ThreadHandle thread, String *str);
OVUM_API String *String_ToLower(ThreadHandle thread, String *str);

OVUM_API String *String_Concat(ThreadHandle thread, const String *a, const String *b);
OVUM_API String *String_Concat3(ThreadHandle thread, const String *a, const String *b, const String *c);
OVUM_API String *String_ConcatRange(ThreadHandle thread, const unsigned int count, String *values[]);

// Converts a String* to a zero-terminated wchar_t* string.
//   dest:
//     The buffer into which to put the output. This must be large enough to
//     contain the resulting string.
//     To get the length of that string (in wchar_t characters, including the
//     terminating \0), pass NULL into this parameter.
//
//   Return value:
//     The length of the resulting string, in number of wchar_t characters,
//     including the terminating \0.
//
// NOTE: the source string may contain \0 characters. These are NOT stripped!
OVUM_API const int String_ToWString(wchar_t *dest, const String *source);

// Converts a zero-terminated char* string to a String*.
//   thread:
//     A handle to the thread on which to throw errors if they occur.
//     If this is NULL, errors will be thrown using C++ 'throw'.
//
//   source:
//     The source string.
//
//   Return value:
//     A GC-managed String*. In native-code methods without managed locals
//     or arguments, set the STR_STATIC flag on the string to prevent the GC
//     from collecting the string if a GC cycle is triggered.
OVUM_API String *String_FromCString(ThreadHandle thread, const char *source);

// Converts a zero-terminated wchar_t* string to a String*.
//   thread:
//     A handle to the thread on which to throw errors if they occur.
//     If this is NULL, errors will be thrown using C++ 'throw'.
//
//   source:
//     The source string.
//
//   Return value:
//     A GC-managed String*. In native-code methods without managed locals
//     or arguments, set the STR_STATIC flag on the string to prevent the GC
//     from collecting the string if a GC cycle is triggered.
OVUM_API String *String_FromWString(ThreadHandle thread, const wchar_t *source);

#endif // VM__STRING_H