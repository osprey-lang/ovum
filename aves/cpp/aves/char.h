#ifndef AVES__CHAR_H
#define AVES__CHAR_H

#include "../aves.h"
#include <ov_unicode.h>

class Char
{
public:
	static LitString<2> ToLitString(const ovwchar_t ch);

	static ovwchar_t FromValue(Value *value);
};

AVES_API NATIVE_FUNCTION(aves_Char_get_length);

AVES_API NATIVE_FUNCTION(aves_Char_get_category);

AVES_API NATIVE_FUNCTION(aves_Char_toUpper);
AVES_API NATIVE_FUNCTION(aves_Char_toLower);

AVES_API NATIVE_FUNCTION(aves_Char_getHashCode);
AVES_API NATIVE_FUNCTION(aves_Char_toString);

AVES_API NATIVE_FUNCTION(aves_Char_fromCodepoint);

AVES_API NATIVE_FUNCTION(aves_Char_opEquals);
AVES_API NATIVE_FUNCTION(aves_Char_opCompare);
AVES_API NATIVE_FUNCTION(aves_Char_opMultiply);
AVES_API NATIVE_FUNCTION(aves_Char_opPlus);

#endif // AVES__CHAR_H
