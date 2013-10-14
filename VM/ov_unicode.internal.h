#pragma once

#ifndef VM__UNICODE_INTERNAL_H
#define VM__UNICODE_INTERNAL_H

#include "ov_vm.internal.h"
#include "ov_unicode.h"

inline CaseMap operator+(const CaseMap map, const int32_t codepoint)
{
	const CaseMap output = { map.upper + codepoint, map.lower + codepoint };
	return output;
}

inline CaseMap operator+(const int32_t codepoint, const CaseMap map)
{
	return map + codepoint;
}

extern const UnicodeCategory CategoryChunks[];
extern const uint16_t IndexMap1[];
extern const uint8_t IndexMap2[];
extern const uint8_t PrimaryMap[];

const UnicodeCategory UC_GetCategoryInternal(const int32_t codepoint);
const CaseMap UC_GetCaseMapInternal(const int32_t codepoint);

extern const int32_t CaseMaps[];
extern const uint8_t CaseIndexMap[];
extern const uint8_t PrimaryCaseMap[];

#endif // VM__UNICODE_INTERNAL_H