#ifndef AVES__CHAR_H
#define AVES__CHAR_H

#include "../aves.h"
#include <ovum_unicode.h>

class Char
{
public:
	static LitString<2> ToLitString(ovwchar_t ch);

	static ovwchar_t FromValue(Value *value);

	static int FromCodepoint(ThreadHandle thread, Value *codepoint, Value *result);
};

AVES_API int OVUM_CDECL aves_Char_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_Char_new);

AVES_API NATIVE_FUNCTION(aves_Char_get_length);
AVES_API NATIVE_FUNCTION(aves_Char_get_category);
AVES_API NATIVE_FUNCTION(aves_Char_get_codePoint);

AVES_API NATIVE_FUNCTION(aves_Char_toUpper);
AVES_API NATIVE_FUNCTION(aves_Char_toLower);
AVES_API NATIVE_FUNCTION(aves_Char_getHashCode);
AVES_API NATIVE_FUNCTION(aves_Char_toString);
AVES_API NATIVE_FUNCTION(aves_Char_fromCodePoint);

AVES_API NATIVE_FUNCTION(aves_Char_opEquals);
AVES_API NATIVE_FUNCTION(aves_Char_opCompare);
AVES_API NATIVE_FUNCTION(aves_Char_opMultiply);

#endif // AVES__CHAR_H
