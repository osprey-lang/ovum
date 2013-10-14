#include "ov_unicode.internal.h"

const UnicodeCategory UC_GetCategoryInternal(const int32_t codepoint)
{
	int32_t index = PrimaryMap[codepoint >> 11];
	index = IndexMap2[(index << 4) + ((codepoint >> 7) & 15)];
	index = IndexMap1[(index << 4) + ((codepoint >> 3) & 15)];
	return CategoryChunks[(index << 3) + (codepoint & 7)];
}

const CaseMap UC_GetCaseMapInternal(const int32_t codepoint)
{
	const CaseMap *cases = reinterpret_cast<const CaseMap*>(CaseMaps);
	int32_t index = PrimaryCaseMap[codepoint >> 13];
	index = CaseIndexMap[(index << 7) + ((codepoint >> 6) & 127)];
	CaseMap map = cases[(index << 6) + (codepoint & 63)];

	return map + codepoint;
}

OVUM_API UnicodeCategory UC_GetCategory(const uchar ch)
{
	return UC_GetCategoryInternal(ch);
}

OVUM_API UnicodeCategory UC_GetCategoryW(const wuchar ch)
{
	assert_valid_wuchar(ch);

	return UC_GetCategoryInternal(ch);
}

OVUM_API CaseMap UC_GetCaseMap(const uchar ch)
{
	return UC_GetCaseMapInternal(ch);
}

OVUM_API CaseMap UC_GetCaseMapW(const wuchar ch)
{
	assert_valid_wuchar(ch);

	return UC_GetCaseMapInternal(ch);
}