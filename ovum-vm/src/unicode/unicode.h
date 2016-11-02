#pragma once

#include "../vm.h"
#include "../../inc/ovum_unicode.h"

namespace ovum
{

namespace unicode
{

	struct CaseOffsets
	{
		int32_t upper;
		int32_t lower;
	};

	inline CaseMap operator+(const CaseOffsets &map, const int32_t codepoint)
	{
		const CaseMap output = { map.upper + codepoint, map.lower + codepoint };
		return output;
	}

	inline CaseMap operator+(const int32_t codepoint, const CaseOffsets &map)
	{
		return map + codepoint;
	}

	UnicodeCategory GetCategory(int32_t codepoint);
	CaseMap GetCaseMap(int32_t codepoint);

	namespace categories
	{
		extern const UnicodeCategory Categories[];
		extern const uint16_t IndexMap1[];
		extern const uint8_t IndexMap2[];
		extern const uint8_t PrimaryMap[];
	}

	namespace cases
	{
		extern const int32_t CaseMaps[];
		extern const uint8_t IndexMap[];
		extern const uint8_t PrimaryMap[];
	}

} // namespace unicode

} // namespace ovum
