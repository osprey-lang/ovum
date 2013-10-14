#pragma once

#ifndef VM__STRING_H
#define VM__STRING_H

#include "ov_vm.h"

/* Available string hash algorithm implementations:
 *   1 – shameless .NET Framework steal, basically
 *   2 – shameless Mono steal, mostly
 *   3 – FNV-1a
 * If you do not select an algorithm, you'll get the lose-lose algorithm,
 * which will ensure huge numbers of collisions, and you have no one to
 * blame but yourself for not reading properly.
 */
#define STRING_HASH_ALGORITHM  3

inline int32_t String_GetHashCode(const int32_t length, const uchar *s)
{
#if STRING_HASH_ALGORITHM == 1
	// Rip of one of the algorithms in the .NET Framework
	int32_t hash1 = (5381 << 16) + 5381;
	int32_t hash2 = hash1;

	while (*s)
	{
		hash1 = ((hash1 << 5) + hash1) ^ *s;

		if (*s++ == 0)
			break;

		hash2 = ((hash2 << 5) + hash2) ^ *s; 
		*s++;
	}

	return hash1 + (hash2 * 1566083941);
#elif STRING_HASH_ALGORITHM == 2
	// Rip of the Mono algorithm, ever so slightly modified
	int32_t hash = 0;

	const uchar *end = s + length - 1;
	while (s < end)
	{
		hash = (hash << 5) - hash + s[0];
		hash = (hash << 5) - hash + s[1];
		s += 2;
	}
	++end;
	if (s < end)
		hash = (hash << 5) - hash + *s;
	return hash;
#elif STRING_HASH_ALGORITHM == 3
	// FNV-1a
	// Note that this operates on a BYTE basis, not character
	int32_t hash = 0x811c9dc5;
	const int32_t prime = 0x01000193;

	//const uint8_t *data = reinterpret_cast<const uint8_t*>(s);
	int32_t remaining = length;
	while (remaining--)
	{
		hash = ((*s & 0xff) ^ hash) * prime;
		hash = ((*s >> 8) ^ hash) * prime;
		s++;
	}

	return hash;
#else
	// Well okay. You didn't specify a hash algorithm, suit yourself.
	int32_t hash = 0;
	int32_t remaining = length;
	while (remaining--)
		hash += *s;
	return hash;
#endif
}
OVUM_API int32_t String_GetHashCode(String *str);

OVUM_API bool String_Equals(const String *a, const String *b);
OVUM_API bool String_EqualsIgnoreCase(const String *a, const String *b);

OVUM_API bool String_SubstringEquals(const String *str, const int32_t startIndex, const String *part);

OVUM_API int String_Compare(const String *a, const String *b);

inline bool String_StartsWith(const String *a, const uchar ch)
{
	return a->firstChar == ch;
}
inline bool String_EndsWith(const String *a, const uchar ch)
{
	return (&a->firstChar)[a->length - 1] == ch;
}

inline bool String_ContainsChar(const String *str, const uchar ch)
{
	for (int32_t i = 0; i < str->length; i++)
		if ((&str->firstChar)[i] == ch)
			return true;
	return false;
}

OVUM_API bool String_Contains(const String *str, const String *value);

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
//     terminating \0), pass null into this parameter.
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
//     If this is null, errors will be thrown using C++ 'throw'.
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
//     If this is null, errors will be thrown using C++ 'throw'.
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