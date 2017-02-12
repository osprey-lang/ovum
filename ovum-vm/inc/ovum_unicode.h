#ifndef OVUM__UNICODE_H
#define OVUM__UNICODE_H

#include "ovum.h"

// Each value is a Unicode general category. Categories are made up of two
// values: one byte that defines the "top" category, such as Letter, Mark,
// Number or similar; and a second byte which, together with the first,
// defines the general category.
enum UnicodeCategory : uint32_t
{
	UC_TOP_CATEGORY_MASK = 0xF0,
	UC_SUB_CATEGORY_MASK = 0x0F,

	UC_LETTER              = 0x10, // L
	UC_LETTER_UPPERCASE    = 0x11, // Lu
	UC_LETTER_LOWERCASE    = 0x12, // Ll
	UC_LETTER_TITLECASE    = 0x13, // Lt
	UC_LETTER_MODIFIER     = 0x14, // Lm
	UC_LETTER_OTHER        = 0x15, // Lo

	UC_MARK                = 0x20, // M
	UC_MARK_NONSPACING     = 0x21, // Mn
	UC_MARK_SPACING        = 0x22, // Mc
	UC_MARK_ENCLOSING      = 0x23, // Me

	UC_NUMBER              = 0x30, // N
	UC_NUMBER_DECIMAL      = 0x31, // Nd
	UC_NUMBER_LETTER       = 0x32, // Nl
	UC_NUMBER_OTHER        = 0x33, // No

	UC_PUNCT               = 0x40, // P
	UC_PUNCT_CONNECTOR     = 0x41, // Pc
	UC_PUNCT_DASH          = 0x42, // Pd
	UC_PUNCT_OPEN          = 0x43, // Ps
	UC_PUNCT_CLOSE         = 0x44, // Pe
	UC_PUNCT_INITIAL       = 0x45, // Pi
	UC_PUNCT_FINAL         = 0x46, // Pf
	UC_PUNCT_OTHER         = 0x47, // Po

	UC_SYMBOL              = 0x50, // S
	UC_SYMBOL_MATH         = 0x51, // Sm
	UC_SYMBOL_CURRENCY     = 0x52, // Sc
	UC_SYMBOL_MODIFIER     = 0x53, // Sk
	UC_SYMBOL_OTHER        = 0x54, // So

	UC_SEPARATOR           = 0x60, // Z
	UC_SEPARATOR_SPACE     = 0x61, // Zs
	UC_SEPARATOR_LINE      = 0x62, // Zl
	UC_SEPARATOR_PARAGRAPH = 0x63, // Zp
			
	UC_OTHER               = 0x70, // C
	UC_CONTROL             = 0x71, // Cc
	UC_FORMAT              = 0x72, // Cf
	UC_SURROGATE           = 0x73, // Cs
	UC_PRIVATE_USE         = 0x74, // Co
	UC_UNASSIGNED          = 0x75, // Cn
};

// A "wide" Unicode character. This is basically the 32-bit version of ovchar_t.
// wchar_t is not used because it is not guaranteed to be any particular size.
//
// Note: ovwchar_t is only used in the unicode.* files because all strings are
// UTF-16 elsewhere.
typedef uint32_t ovwchar_t;

typedef struct CaseMap_S
{
	ovwchar_t upper;
	ovwchar_t lower;
} CaseMap;

typedef struct SurrogatePair_S
{
	ovchar_t lead;
	ovchar_t trail;
} SurrogatePair;

// Gets the Unicode general category of the specified UTF-16 code unit.
OVUM_API UnicodeCategory UC_GetCategory(ovchar_t ch);
// Gets a case map for the specified UTF-16 code unit. A case map contains
// the uppercase and lowercase mappings of a given Unicode code point.
OVUM_API CaseMap UC_GetCaseMap(ovchar_t ch);

// Gets the Unicode general category of the specified code point.
OVUM_API UnicodeCategory UC_GetCategoryW(ovwchar_t ch);
// Gets a case map for the specified code point. A case map contains
// the uppercase and lowercase mappings of a given Unicode code point.
OVUM_API CaseMap UC_GetCaseMapW(ovwchar_t ch);

#define OVUM_ASSERT_NON_BMP(ch) OVUM_ASSERT((ch) >= 0x10000 && (ch) <= 0x10FFFF)

// UTF-16 code unit functions

inline bool UC_IsSurrogateLead(ovchar_t ch)
{
	return ch >= 0xD800 && ch <= 0xDBFF;
}
inline bool UC_IsSurrogateTrail(ovchar_t ch)
{
	return ch >= 0xDC00 && ch <= 0xDFFF;
}

inline ovwchar_t UC_ToWide(ovchar_t lead, ovchar_t trail)
{
	return 0x10000 + (((ovwchar_t)lead - 0xD800) << 10) + (ovwchar_t)trail - 0xDC00;
}
inline ovwchar_t UC_ToWide(SurrogatePair pair)
{
	return UC_ToWide(pair.lead, pair.trail);
}

inline bool UC_IsCategory(ovchar_t ch, UnicodeCategory cat)
{
	UnicodeCategory charCat = UC_GetCategory(ch);
	if ((cat & UC_SUB_CATEGORY_MASK) == 0)
		return (charCat & UC_TOP_CATEGORY_MASK) == cat;
	else
		return charCat == cat;
}
inline bool UC_IsUpper(ovchar_t ch)
{
	return UC_GetCategory(ch) == UC_LETTER_UPPERCASE;
}
inline bool UC_IsLower(ovchar_t ch)
{
	return UC_GetCategory(ch) == UC_LETTER_LOWERCASE;
}

inline ovchar_t UC_ToUpper(ovchar_t ch)
{
	return (ovchar_t)UC_GetCaseMap(ch).upper;
}
inline ovchar_t UC_ToLower(ovchar_t ch)
{
	return (ovchar_t)UC_GetCaseMap(ch).lower;
}

// "True" Unicode functions

inline bool UC_IsCategory(ovwchar_t ch, UnicodeCategory cat)
{
	UnicodeCategory charCat = UC_GetCategoryW(ch);
	if ((cat & UC_SUB_CATEGORY_MASK) == 0)
		return (charCat & UC_TOP_CATEGORY_MASK) == cat;
	else
		return charCat == cat;
}
inline bool UC_IsUpper(ovwchar_t ch)
{
	return UC_GetCategoryW(ch) == UC_LETTER_UPPERCASE;
}
inline bool UC_IsLower(ovwchar_t ch)
{
	return UC_GetCategoryW(ch) == UC_LETTER_LOWERCASE;
}

inline ovwchar_t UC_ToUpper(ovwchar_t ch)
{
	return UC_GetCaseMapW(ch).upper;
}
inline ovwchar_t UC_ToLower(ovwchar_t ch)
{
	return UC_GetCaseMapW(ch).lower;
}

inline bool UC_NeedsSurrogatePair(ovwchar_t ch)
{
	return ch > 0xFFFF;
}

inline SurrogatePair UC_ToSurrogatePair(ovwchar_t ch)
{
	OVUM_ASSERT_NON_BMP(ch);
	ovwchar_t ch2 = ch - 0x10000;
	SurrogatePair output = { 0xD800 + ((ch2 >> 10) & 0x3FF), 0xDC00 + (ch2 & 0x3FF) };
	return output;
}

// UTF-16 array functions

inline UnicodeCategory UC_GetCategory(const ovchar_t chars[], size_t index, bool *wasSurrogatePair)
{
	const ovchar_t first = chars[index];
	*wasSurrogatePair = UC_IsSurrogateLead(first) && UC_IsSurrogateTrail(chars[index + 1]);
	if (*wasSurrogatePair)
		return UC_GetCategoryW(UC_ToWide(first, chars[index + 1]));
	else
		return UC_GetCategory(first);
}
inline UnicodeCategory UC_GetCategory(const ovchar_t chars[], size_t index)
{
	bool ignore;
	return UC_GetCategory(chars, index, &ignore);
}

inline bool UC_IsCategory(const ovchar_t chars[], size_t index, UnicodeCategory cat, bool *wasSurrogatePair)
{
	UnicodeCategory charCat = UC_GetCategory(chars, index, wasSurrogatePair);
	if ((cat & UC_SUB_CATEGORY_MASK) == 0)
		return (charCat & UC_TOP_CATEGORY_MASK) == cat;
	else
		return charCat == cat;
}
inline bool UC_IsCategory(const ovchar_t chars[], size_t index, UnicodeCategory cat)
{
	bool ignore;
	return UC_IsCategory(chars, index, cat, &ignore);
}

inline bool UC_IsUpper(const ovchar_t chars[], size_t index, bool *wasSurrogatePair)
{
	return UC_IsCategory(chars, index, UC_LETTER_UPPERCASE, wasSurrogatePair);
}
inline bool UC_IsUpper(const ovchar_t chars[], size_t index)
{
	bool ignore;
	return UC_IsUpper(chars, index, &ignore);
}

inline bool UC_IsLower(const ovchar_t chars[], size_t index, bool *wasSurrogatePair)
{
	return UC_IsCategory(chars, index, UC_LETTER_LOWERCASE, wasSurrogatePair);
}
inline bool UC_IsLower(const ovchar_t chars[], size_t index)
{
	bool ignore;
	return UC_IsLower(chars, index, &ignore);
}

#undef assert_valid_wuchar

#endif // OVUM__UNICODE_H
