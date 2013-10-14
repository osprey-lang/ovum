#pragma once

#ifndef AVES__STRING_H
#define AVES__STRING_H

#include "aves.h"

AVES_API NATIVE_FUNCTION(aves_String_get_item);

AVES_API NATIVE_FUNCTION(aves_String_get_length);

AVES_API NATIVE_FUNCTION(aves_String_equalsIgnoreCase);
AVES_API NATIVE_FUNCTION(aves_String_contains);

AVES_API NATIVE_FUNCTION(aves_String_reverse);
AVES_API NATIVE_FUNCTION(aves_String_substr1);
AVES_API NATIVE_FUNCTION(aves_String_substr2);
AVES_API NATIVE_FUNCTION(aves_String_format);
AVES_API NATIVE_FUNCTION(aves_String_replaceInner);

AVES_API NATIVE_FUNCTION(aves_String_toUpper);
AVES_API NATIVE_FUNCTION(aves_String_toLower);

AVES_API NATIVE_FUNCTION(aves_String_getCategory);

AVES_API NATIVE_FUNCTION(aves_String_getHashCode);

AVES_API NATIVE_FUNCTION(aves_String_fromCodepoint);

AVES_API NATIVE_FUNCTION(aves_String_opEquals);
AVES_API NATIVE_FUNCTION(aves_String_opCompare);
AVES_API NATIVE_FUNCTION(aves_String_opMultiply);

// Internal methods

namespace string
{
	String *Format(ThreadHandle thread, const String *format, ListInst *list);
	String *Format(ThreadHandle thread, const String *format, Value *hash);

	String *Replace(ThreadHandle thread, const String *input, const uchar oldChar, const uchar newChar, const int64_t maxTimes);
	String *Replace(ThreadHandle thread, const String *input, String *oldValue, String *newValue, const int64_t maxTimes);
}

#endif // AVES__STRING_H