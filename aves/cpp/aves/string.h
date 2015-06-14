#ifndef AVES__STRING_H
#define AVES__STRING_H

#include "../aves.h"
#include <ov_unicode.h>

AVES_API NATIVE_FUNCTION(aves_String_get_item);

AVES_API NATIVE_FUNCTION(aves_String_get_length);

AVES_API NATIVE_FUNCTION(aves_String_get_isInterned);

AVES_API NATIVE_FUNCTION(aves_String_equalsIgnoreCase);
AVES_API NATIVE_FUNCTION(aves_String_contains);
AVES_API NATIVE_FUNCTION(aves_String_startsWith);
AVES_API NATIVE_FUNCTION(aves_String_endsWith);
AVES_API NATIVE_FUNCTION(aves_String_indexOf);
AVES_API NATIVE_FUNCTION(aves_String_lastIndexOf);

AVES_API NATIVE_FUNCTION(aves_String_reverse);
AVES_API NATIVE_FUNCTION(aves_String_substr1);
AVES_API NATIVE_FUNCTION(aves_String_substr2);
AVES_API NATIVE_FUNCTION(aves_String_format);
AVES_API NATIVE_FUNCTION(aves_String_replaceInner);
AVES_API NATIVE_FUNCTION(aves_String_split);

AVES_API NATIVE_FUNCTION(aves_String_padInner);

AVES_API NATIVE_FUNCTION(aves_String_toUpper);
AVES_API NATIVE_FUNCTION(aves_String_toLower);

AVES_API NATIVE_FUNCTION(aves_String_getCharacter);
AVES_API NATIVE_FUNCTION(aves_String_getCodepoint);

AVES_API NATIVE_FUNCTION(aves_String_getCategory);
AVES_API NATIVE_FUNCTION(aves_String_isSurrogatePair);

AVES_API NATIVE_FUNCTION(aves_String_getInterned);
AVES_API NATIVE_FUNCTION(aves_String_intern);

AVES_API NATIVE_FUNCTION(aves_String_getHashCode);
AVES_API NATIVE_FUNCTION(aves_String_getHashCodeSubstr);

AVES_API NATIVE_FUNCTION(aves_String_fromCodepoint);

AVES_API NATIVE_FUNCTION(aves_String_opEquals);
AVES_API NATIVE_FUNCTION(aves_String_opCompare);
AVES_API NATIVE_FUNCTION(aves_String_opMultiply);

// Note: These values must be synchronised with aves.StringPad (in String.osp)
enum StringPad
{
	PAD_START = 1,
	PAD_END = 2,
	PAD_BOTH = 3,
};

// Internal methods

namespace unicode
{
	inline uint32_t OvumCategoryToAves(UnicodeCategory cat)
	{
		switch (cat)
		{
			case UC_LETTER_UPPERCASE:    return 1 <<  0;
			case UC_LETTER_LOWERCASE:    return 1 <<  1;
			case UC_LETTER_TITLECASE:    return 1 <<  2;
			case UC_LETTER_MODIFIER:     return 1 <<  3;
			case UC_LETTER_OTHER:        return 1 <<  4;
			case UC_MARK_NONSPACING:     return 1 <<  5;
			case UC_MARK_SPACING:        return 1 <<  6;
			case UC_MARK_ENCLOSING:      return 1 <<  7;
			case UC_NUMBER_DECIMAL:      return 1 <<  8;
			case UC_NUMBER_LETTER:       return 1 <<  9;
			case UC_NUMBER_OTHER:        return 1 << 10;
			case UC_PUNCT_CONNECTOR:     return 1 << 11;
			case UC_PUNCT_DASH:          return 1 << 12;
			case UC_PUNCT_OPEN:          return 1 << 13;
			case UC_PUNCT_CLOSE:         return 1 << 14;
			case UC_PUNCT_INITIAL:       return 1 << 15;
			case UC_PUNCT_FINAL:         return 1 << 16;
			case UC_PUNCT_OTHER:         return 1 << 17;
			case UC_SYMBOL_MATH:         return 1 << 18;
			case UC_SYMBOL_CURRENCY:     return 1 << 19;
			case UC_SYMBOL_MODIFIER:     return 1 << 20;
			case UC_SYMBOL_OTHER:        return 1 << 21;
			case UC_SEPARATOR_SPACE:     return 1 << 22;
			case UC_SEPARATOR_LINE:      return 1 << 23;
			case UC_SEPARATOR_PARAGRAPH: return 1 << 24;
			case UC_CONTROL:             return 1 << 25;
			case UC_FORMAT:              return 1 << 26;
			case UC_SURROGATE:           return 1 << 27;
			case UC_PRIVATE_USE:         return 1 << 28;
			case UC_UNASSIGNED:          return 1 << 29;
			default:                     return 0;
		}
	}
}

namespace string
{
	int32_t IndexOf(const String *str, const String *part);
	int32_t LastIndexOf(const String *str, const String *part);

	int Format(ThreadHandle thread, const String *format, ListInst *list, String *&result);
	int Format(ThreadHandle thread, const String *format, Value *hash, String *&result);

	String *Replace(ThreadHandle thread, String *input, const ovchar_t oldChar, const ovchar_t newChar, const int64_t maxTimes);
	String *Replace(ThreadHandle thread, String *input, String *oldValue, String *newValue, const int64_t maxTimes);
}

#endif // AVES__STRING_H