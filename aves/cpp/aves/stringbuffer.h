#ifndef AVES__STRINGBUFFER_H
#define AVES__STRINGBUFFER_H

#include "../aves.h"

AVES_API void aves_StringBuffer_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_StringBuffer_new);
AVES_API NATIVE_FUNCTION(aves_StringBuffer_newCap);

AVES_API NATIVE_FUNCTION(aves_StringBuffer_get_item);
AVES_API NATIVE_FUNCTION(aves_StringBuffer_set_item);
AVES_API NATIVE_FUNCTION(aves_StringBuffer_get_length);
AVES_API NATIVE_FUNCTION(aves_StringBuffer_get_capacity);

AVES_API NATIVE_FUNCTION(aves_StringBuffer_appendLine);
AVES_API NATIVE_FUNCTION(aves_StringBuffer_append);
AVES_API NATIVE_FUNCTION(aves_StringBuffer_appendCodePoint);
AVES_API NATIVE_FUNCTION(aves_StringBuffer_appendSubstringFromString);
AVES_API NATIVE_FUNCTION(aves_StringBuffer_appendSubstringFromBuffer);
AVES_API NATIVE_FUNCTION(aves_StringBuffer_insert);
AVES_API NATIVE_FUNCTION(aves_StringBuffer_clear);
AVES_API NATIVE_FUNCTION(aves_StringBuffer_toString);
AVES_API NATIVE_FUNCTION(aves_StringBuffer_toStringSubstring);

void aves_StringBuffer_finalize(void *basePtr);

#endif // AVES__STRINGBUFFER_H
