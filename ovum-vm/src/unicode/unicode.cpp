#include "unicode.h"

namespace ovum
{

namespace unicode
{

	UnicodeCategory GetCategory(int32_t codepoint)
	{
		int32_t index = categories::PrimaryMap[codepoint >> 11];
		index = categories::IndexMap2[(index << 4) + ((codepoint >> 7) & 15)];
		index = categories::IndexMap1[(index << 4) + ((codepoint >> 3) & 15)];
		return categories::Categories[(index << 3) + (codepoint & 7)];
	}

	CaseMap GetCaseMap(int32_t codepoint)
	{
		const CaseOffsets *caseMaps = reinterpret_cast<const CaseOffsets*>(cases::CaseMaps);
		int32_t index = cases::PrimaryMap[codepoint >> 13];
		index = cases::IndexMap[(index << 7) + ((codepoint >> 6) & 127)];
		CaseOffsets offsets = caseMaps[(index << 6) + (codepoint & 63)];

		return offsets + codepoint;
	}

} // namespace unicode

} // namespace ovum

OVUM_API UnicodeCategory UC_GetCategory(uchar ch)
{
	return ovum::unicode::GetCategory(ch);
}

OVUM_API CaseMap UC_GetCaseMap(uchar ch)
{
	return ovum::unicode::GetCaseMap(ch);
}

OVUM_API UnicodeCategory UC_GetCategoryW(wuchar ch)
{
	return ovum::unicode::GetCategory((int32_t)ch);
}

OVUM_API CaseMap UC_GetCaseMapW(wuchar ch)
{
	return ovum::unicode::GetCaseMap((int32_t)ch);
}