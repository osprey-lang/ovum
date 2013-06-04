#pragma once

#ifndef AVES__STRING_H
#define AVES__STRING_H

#include "aves.h"

AVES_API NATIVE_FUNCTION(aves_String_get_item);

AVES_API NATIVE_FUNCTION(aves_String_get_length);

AVES_API NATIVE_FUNCTION(aves_String_getCategory);
AVES_API NATIVE_FUNCTION(aves_String_format);

AVES_API NATIVE_FUNCTION(aves_String_getHashCode);
AVES_API NATIVE_FUNCTION(aves_String_toString);

AVES_API NATIVE_FUNCTION(aves_String_opEquals);
AVES_API NATIVE_FUNCTION(aves_String_opCompare);
AVES_API NATIVE_FUNCTION(aves_String_opMultiply);

// Internal methods

namespace string
{
	String *Format(ThreadHandle thread, const String *format, ListInst *list);
	String *Format(ThreadHandle thread, const String *format, Value *hash);
}

#endif // AVES__STRING_H