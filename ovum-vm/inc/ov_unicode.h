#pragma once

#ifndef VM__UNICODE_H
#define VM__UNICODE_H

#include <cassert>
#include "ov_vm.h"

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

// A "wide" Unicode character. This is basically the 32-bit version of uchar.
// wchar_t is not used because it is not guaranteed to be any particular size.
//
// Note: wuchar is only used in the unicode.* files because all strings are
// UTF-16 elsewhere.
typedef uint32_t wuchar;

typedef struct CaseMap_S
{
	wuchar upper;
	wuchar lower;
} CaseMap;

typedef struct SurrogatePair_S
{
	uchar lead;
	uchar trail;
} SurrogatePair;

// Gets the Unicode general category of the specified UTF-16 code unit.
OVUM_API UnicodeCategory UC_GetCategory(uchar ch);
// Gets a case map for the specified UTF-16 code unit. A case map contains
// the uppercase and lowercase mappings of a given Unicode code point.
OVUM_API CaseMap UC_GetCaseMap(uchar ch);

// Gets the Unicode general category of the specified code point.
OVUM_API UnicodeCategory UC_GetCategoryW(wuchar ch);
// Gets a case map for the specified code point. A case map contains
// the uppercase and lowercase mappings of a given Unicode code point.
OVUM_API CaseMap UC_GetCaseMapW(wuchar ch);

#define assert_valid_wuchar(ch)  assert((ch) >= 0x10000 && (ch) <= 0x10FFFF)

// UTF-16 code unit functions

inline bool UC_IsSurrogateLead(uchar ch)
{
	return ch >= 0xD800 && ch <= 0xDBFF;
}
inline bool UC_IsSurrogateTrail(uchar ch)
{
	return ch >= 0xDC00 && ch <= 0xDFFF;
}

inline wuchar UC_ToWide(uchar lead, uchar trail)
{
	return 0x10000 + (((wuchar)lead - 0xD800) << 10) + (wuchar)trail - 0xDC00;
}
inline wuchar UC_ToWide(SurrogatePair pair)
{
	return UC_ToWide(pair.lead, pair.trail);
}

inline bool UC_IsCategory(uchar ch, UnicodeCategory cat)
{
	UnicodeCategory charCat = UC_GetCategory(ch);
	if ((cat & UC_SUB_CATEGORY_MASK) == 0)
		return (charCat & UC_TOP_CATEGORY_MASK) == cat;
	else
		return charCat == cat;
}
inline bool UC_IsUpper(uchar ch)
{
	return UC_GetCategory(ch) == UC_LETTER_UPPERCASE;
}
inline bool UC_IsLower(uchar ch)
{
	return UC_GetCategory(ch) == UC_LETTER_LOWERCASE;
}

inline uchar UC_ToUpper(uchar ch)
{
	return (uchar)UC_GetCaseMap(ch).upper;
}
inline uchar UC_ToLower(uchar ch)
{
	return (uchar)UC_GetCaseMap(ch).lower;
}

// "True" Unicode functions

inline bool UC_IsCategory(wuchar ch, UnicodeCategory cat)
{
	UnicodeCategory charCat = UC_GetCategoryW(ch);
	if ((cat & UC_SUB_CATEGORY_MASK) == 0)
		return (charCat & UC_TOP_CATEGORY_MASK) == cat;
	else
		return charCat == cat;
}
inline bool UC_IsUpper(wuchar ch)
{
	return UC_GetCategoryW(ch) == UC_LETTER_UPPERCASE;
}
inline bool UC_IsLower(wuchar ch)
{
	return UC_GetCategoryW(ch) == UC_LETTER_LOWERCASE;
}

inline wuchar UC_ToUpper(wuchar ch)
{
	return UC_GetCaseMapW(ch).upper;
}
inline wuchar UC_ToLower(wuchar ch)
{
	return UC_GetCaseMapW(ch).lower;
}

inline bool UC_NeedsSurrogatePair(wuchar ch)
{
	return ch > 0xFFFF;
}

inline SurrogatePair UC_ToSurrogatePair(wuchar ch)
{
	assert_valid_wuchar(ch);
	wuchar ch2 = ch - 0x10000;
	SurrogatePair output = { 0xD800 + ((ch2 >> 10) & 0x3FF), 0xDC00 + (ch2 & 0x3FF) };
	return output;
}

// UTF-16 array functions

inline UnicodeCategory UC_GetCategory(const uchar chars[], unsigned int index, bool *wasSurrogatePair)
{
	const uchar first = chars[index];
	*wasSurrogatePair = UC_IsSurrogateLead(first) && UC_IsSurrogateTrail(chars[index + 1]);
	if (*wasSurrogatePair)
		return UC_GetCategoryW(UC_ToWide(first, chars[index + 1]));
	else
		return UC_GetCategory(first);
}
inline UnicodeCategory UC_GetCategory(const uchar chars[], unsigned int index)
{
	bool ignore;
	return UC_GetCategory(chars, index, &ignore);
}

inline bool UC_IsCategory(const uchar chars[], unsigned int index, UnicodeCategory cat, bool *wasSurrogatePair)
{
	UnicodeCategory charCat = UC_GetCategory(chars, index, wasSurrogatePair);
	if ((cat & UC_SUB_CATEGORY_MASK) == 0)
		return (charCat & UC_TOP_CATEGORY_MASK) == cat;
	else
		return charCat == cat;
}
inline bool UC_IsCategory(const uchar chars[], unsigned int index, UnicodeCategory cat)
{
	bool ignore;
	return UC_IsCategory(chars, index, cat, &ignore);
}

inline bool UC_IsUpper(const uchar chars[], unsigned int index, bool *wasSurrogatePair)
{
	return UC_IsCategory(chars, index, UC_LETTER_UPPERCASE, wasSurrogatePair);
}
inline bool UC_IsUpper(const uchar chars[], unsigned int index)
{
	bool ignore;
	return UC_IsUpper(chars, index, &ignore);
}

inline bool UC_IsLower(const uchar chars[], unsigned int index, bool *wasSurrogatePair)
{
	return UC_IsCategory(chars, index, UC_LETTER_LOWERCASE, wasSurrogatePair);
}
inline bool UC_IsLower(const uchar chars[], unsigned int index)
{
	bool ignore;
	return UC_IsLower(chars, index, &ignore);
}

#undef assert_valid_wuchar

#endif // VM__UNICODE_H